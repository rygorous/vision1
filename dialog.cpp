#include "common.h"
#include "dialog.h"
#include "vars.h"
#include "util.h"
#include "graphics.h"
#include "font.h"
#include "script.h"
#include "main.h"
#include "mouse.h"
#include <vector>
#include <assert.h>

static const int LABEL_LEN = 5;

namespace {
    // this is the raw file format
    struct DialogString {
        U8 text_len;            // 0
        U8 var_index;           // 1
        U8 unk[2];              // 2-3
        U8 label[LABEL_LEN];    // 4-8
        U8 unk2;                // 9
        U8 text[1];             // 10; actually text_len bytes
    };

    class Dialog {
        std::string charname;
        std::vector<int> dir;
        int root;
        Slice data;

        int find_label_rec(int item, const U8 label[LABEL_LEN]) const;
        int find_label(const char *which, const char *whichend) const;
        const DialogString *decode(int item) const;

    public:
        Dialog(const char *charname);

        void load(const char *dlgname);
        int get_root() const;
        int get_next(int item, int which) const;
        int decode_and_follow(int state, const DialogString *&str);
        
        void debug_dump(int item);
        void debug_dump_all();
    };
}

int Dialog::find_label_rec(int item, const U8 label[LABEL_LEN]) const
{
    const DialogString *str = decode(item);
    if (str && memcmp(str->label, label, LABEL_LEN) == 0)
        return item;

    for (int i=0; i < 5; i++) {
        if (int link = get_next(item, i)) {
            if (int res = find_label_rec(link, label))
                return res;
        }
    }
    return 0;
}

int Dialog::find_label(const char *which, const char *whichend) const
{
    U8 buf[LABEL_LEN];
    int i;

    for (i=0; i < LABEL_LEN && which + i < whichend; i++)
        buf[i] = which[i];
    while (i < LABEL_LEN)
        buf[i++] = ' ';
    return find_label_rec(root, buf);
}

const DialogString *Dialog::decode(int item) const
{
    if (!item)
        return nullptr;
    int offs = dir.at(item + 7);
    return (offs == 0xffff) ? nullptr : (const DialogString *)&data[offs];
}

Dialog::Dialog(const char *charname)
    : charname(charname)
{
}

void Dialog::load(const char *dlgname)
{
    char filename[128];
    sprintf(filename, "chars/%s/%s.cm", charname.c_str(), dlgname);

    Slice file = read_xored(filename);
    int dirwords = little_u16(&file[0]);
    int datasize = little_u16(&file[2]);
    root = little_u16(&file[4]);
    dir.resize(dirwords);
    for (int i=0; i < dirwords; i++)
        dir[i] = little_u16(&file[i*2]);
    data = file(dirwords*2, dirwords*2+datasize);
}

int Dialog::get_root() const
{
    return root;
}

int Dialog::get_next(int item, int which) const
{
    if (which < 0 || which > 5)
        return 0;
    else
        return dir[item + 1 + which];
}

int Dialog::decode_and_follow(int state, const DialogString *&str)
{
    while (state) {
        str = decode(state);
        if (!str)
            break;
        if (str->text_len == 0 || str->text[0] != '^')
            return state;

        // it's a link, follow it
        const char *target = (const char *)str->text + 1;
        const char *targetend = (const char *)str->text + str->text_len;
        if (const char *colon = (const char *)memchr(str->text, ':', str->text_len)) {
            char dlgname[256];
            int dlglen = colon - target;
            assert(dlglen < ARRAY_COUNT(dlgname));

            memcpy(dlgname, str->text, dlglen);
            dlgname[colon - target] = 0;
            load(dlgname);

            target = colon + 1;
        }
        state = find_label(target, targetend);
    }

    // no line
    str = nullptr;
    return state;
}

void Dialog::debug_dump(int item)
{
    printf("node     = %d\n", item);
    printf("links    = [");
    for (int i=0; i < 8; i++)
        printf(" %d", dir[item+i]);
    printf(" ]\n");

    const DialogString *str = decode(item);
    if (str) {
        printf("var_index= %d\n", str->var_index);
        for (int i=0; i < 2; i++)
            printf("unk[%d]   = %d\n", i, str->unk[i]);
        printf("label    = \"%.*s\"\n", LABEL_LEN, str->label);
        printf("unk2     = %d\n", str->unk2);
        printf("text     = \"%.*s\"\n", str->text_len, str->text);
    }
    printf("\n");
}

void Dialog::debug_dump_all()
{
    for (size_t i=5; i < dir.size(); i += 8)
        debug_dump(i);
}

static void display_face(const char *charname)
{
    char filename[128];
    sprintf(filename, "chars/%s/face.frz", charname);
    Slice s = read_file(filename);

    // use top 128 palette entries from .frz
    // but keep topmost 8 as they are (used for text)
    memcpy(&palette_a[128], &s[128*sizeof(PalEntry)], 0x78*sizeof(PalEntry));
    memcpy(&palette_b[128], &s[128*sizeof(PalEntry)], 0x78*sizeof(PalEntry));
    decode_delta(vga_screen, &s[256*sizeof(PalEntry)]);
    set_palette();
}

static int word_end(const U8 *text, int start, int len)
{
    int i = start;
    while (i < len && text[i] != ' ')
        i++;
    return i;
}

static bool is_punctuation(char ch)
{
    return (ch == '.' || ch == '!' || ch == '?' || ch == ',' || ch == ';' || ch == ':' || ch == ' ');
}

static void interpolate_line(std::string &out, std::vector<size_t> &breaks, const U8 *text, int len)
{
    int i = 0;
    breaks.push_back(0);

    while (i < len) {
        switch (text[i]) {
        case ' ': // space
            breaks.push_back(out.size()); // a space is a break point
            out.push_back(text[i++]); // and also a real character
            break;
        case '@': { // interpolated variable
            int end = word_end(text, i + 1, len);
            std::string varname((char*)text + i + 1, (char *)text+end);
            out += get_var_as_str(varname);
            i = end;
            if (i+1 < len && is_punctuation(text[i+1]))
                i++;
            } break;
        case '-': // might be a real dash or just a hyphenation point
            if (i + 1 < len && text[i+1] >= 'a' && text[i+1] <= 'z') // looks like a hyphenation point
                breaks.push_back(out.size());
            else {
                out.push_back(text[i]);
                breaks.push_back(out.size());
            }
            i++;
            break;
        default:
            out.push_back(text[i++]);
            break;
        }
    }

    if (!out.empty())
        breaks.push_back(out.size());
}

static int print_text_linebreak(const Font *font, const U8 *text, int len, int x0, int y0, int x1,
    void (*fragment_callback)(void *user, size_t, size_t), void *user)
{
    std::string txt;
    std::vector<size_t> breaks;
    interpolate_line(txt, breaks, text, len);

    int cur_x = x0, cur_y = y0;
    int lineh = 10;
    int spacew = font->glyph_width(' ');
    int hyphenw = font->glyph_width('-');
    bool hashyph, lasthyph = false;

    // chop it all up into lines
    for (size_t brkpos=0; brkpos + 1 < breaks.size(); brkpos++) {
        // width of fragment, plus trailing hyphen if necessary
        size_t start = breaks[brkpos];
        size_t end = breaks[brkpos + 1];
        int width = font->str_width(&txt[start], end-start);
        int layoutw = width;
        hashyph = false;
        if (end < txt.size() && txt[end] != ' ' && txt[end-1] != '-') {
            layoutw += hyphenw;
            hashyph = true;
        }

        // break if we need to
        if (cur_x + layoutw > x1) {
            if (lasthyph)
                font->print(cur_x, cur_y, "-");
            cur_x = x0;
            cur_y += lineh;
            if (txt[start] == ' ') {
                start++;
                width -= spacew;
            }
        }

        // move the mouth for the right number of frames
        if (fragment_callback)
            fragment_callback(user, start, end);

        font->print(cur_x, cur_y, &txt[start], end-start);
        cur_x += width;
        lasthyph = hashyph;
    }

    return cur_y + lineh;
}

static void say_line_callback(void *user, size_t start, size_t end)
{
    Animation *mouth = (Animation *)user;
    for (size_t i = start; i < end; i++) {
        mouth->render();
        mouth->tick();
        if (mouth->is_done())
            mouth->rewind();

        game_frame();
    }
}

static void say_line(Animation *mouth, const U8 *text, int len)
{
    int x = 4, y = 42;
    print_text_linebreak(bigfont, text, len, x, y, x + 144, say_line_callback, mouth);

    mouth->rewind();
    mouth->render();
    game_frame();
}

static int handle_text_input(Dialog &dlg, int state, const DialogString *str)
{
    // TODO implement line editor (but need keyboard events first)
    std::string varname((char *)str->text + 1, (char *)str->text + str->text_len);
    set_var_str(varname, "hund");
    return dlg.get_next(state, 0);
}

static int handle_choices(Dialog &dlg, int state, int *hover)
{
    int choice = -1, new_hover = -1;
    set_mouse_cursor(MC_NORMAL);

    int cur_y = 145;
    for (int i=0; i < 5; i++) {
        const DialogString *str;
        int option = dlg.decode_and_follow(dlg.get_next(state, i), str);
        if (!str) {
            if (i == 0 && (mouse_button & 1)) // no choices - just wait for click
                return option ? dlg.get_next(option, 0) : 0;
            break;
        }

        if (i == 0 && str->text[0] == '@' && str->text[str->text_len-1] == '$')
            return handle_text_input(dlg, option, str);

        int start_y = cur_y;
        const Font *font = (i == *hover) ? bigfont_highlight : bigfont;
        cur_y = print_text_linebreak(font, str->text, str->text_len, 0, cur_y, 320, nullptr, nullptr);

        if (mouse_y >= start_y && mouse_y < cur_y) {
            new_hover = i;
            set_mouse_cursor(MC_TALK);
            if (mouse_button & 1)
                choice = dlg.get_next(option, 0);
        }

        cur_y += 2;
    }

    *hover = new_hover;
    return choice;
}

void run_dialog(const char *charname, const char *dlgname)
{
    SavedScreen saved_scr;

    bool exclam = false;
    if (charname[0] == '!') {
        exclam = true;
        charname++;
    }

    display_face(charname);

    char filename[128];
    sprintf(filename, "chars/%s/sprech.ani", charname);
    Animation *mouth = new_big_anim(filename, false);

    Dialog dlg(charname);
    dlg.load(dlgname);

    printf("===== START FULL DIALOG DUMP\n\n");

    dlg.debug_dump_all();

    printf("\n===== END FULL DIALOG DUMP\n\n");

    sprintf(filename, "chars/%s/%s.vb", charname, dlgname);
    Slice vars = try_read_xored(filename);
    print_hex("vars", vars);

    SavedScreen clean_scr;

    int state = dlg.get_root();
    while (state) {
        clean_scr.restore(); // reset everything to start
        
        const DialogString *str;
        state = dlg.decode_and_follow(state, str);
        if (!state || !str)
            break;

        dlg.debug_dump(state);
        for (int i=0; i < 5; i++)
            dlg.debug_dump(dlg.get_next(state, i));

        say_line(mouth, str->text, str->text_len);
        int choice, hover = -1;
        while ((choice = handle_choices(dlg, state, &hover)) == -1)
            game_frame();

        set_mouse_cursor(MC_NORMAL);
        state = choice;
    }

    delete mouth;
}