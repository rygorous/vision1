#include "common.h"
#include <vector>
#include <assert.h>

static const int LABEL_LEN = 5;

namespace {
    // this is the raw file format
    struct DialogString {
        U8 text_len;
        U8 unk[3];
        U8 label[LABEL_LEN];
        U8 unk2;
        U8 text[1]; // actually text_len bytes
    };

    class Dialog {
        std::string charname;
        std::vector<int> dir;
        int root;
        Slice data;

        int find_label_rec(int item, const U8 label[LABEL_LEN]) const;
        int find_label(const char *which, const char *whichend) const;

    public:
        Dialog(const char *charname);

        void load(const char *dlgname);
        int get_root() const;
        int get_next(int item, int which) const;
        const DialogString *decode(int item) const;
        int decode_and_follow(int state, const DialogString *&str);
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

const DialogString *Dialog::decode(int item) const
{
    int offs = dir.at(item + 7);
    return (offs == 0xffff) ? nullptr : (const DialogString *)&data[offs];
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
        const char *target = (const char *)str->text;
        const char *targetend = target + str->text_len;
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

    // if we get here, it's an error
    str = nullptr;
    return 0;
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
    return (ch == '.' || ch == '!' || ch == '?' || ch == ',' || ch == ';' || ch == ':');
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

    breaks.push_back(out.size());
}

static void say_line(BigAnimation &mouth, const U8 *text, int len)
{
    std::string txt;
    std::vector<size_t> breaks;
    interpolate_line(txt, breaks, text, len);

    // chop it all up into lines
    int min_x = 4, max_x = min_x + 148;
    int cur_x = min_x, cur_y = 42;
    int lineh = 10;
    int spacew = bigfont.glyph_width(' ');
    int hyphenw = bigfont.glyph_width('-');
    bool hashyph, lasthyph = false;

    for (size_t brkpos=0; brkpos + 1 < breaks.size(); brkpos++) {
        // width of fragment, plus trailing hyphen if necessary
        size_t start = breaks[brkpos];
        size_t end = breaks[brkpos + 1];
        int width = bigfont.str_width(&txt[start], end-start);
        int layoutw = width;
        hashyph = false;
        if (end < txt.size() && txt[end] != ' ' && txt[end-1] != '-') {
            layoutw += hyphenw;
            hashyph = true;
        }

        // break if we need to
        if (cur_x + layoutw > max_x) {
            if (lasthyph)
                bigfont.print(cur_x, cur_y, "-");
            cur_x = min_x;
            cur_y += lineh;
            if (txt[start] == ' ') {
                start++;
                width -= spacew;
            }
        }

        // move the mouth for the right number of frames
        for (size_t i = start; i < end; i++) {
            mouth.render();
            mouth.tick();
            if (mouth.is_done())
                mouth.rewind();

            game_frame();
        }

        bigfont.print(cur_x, cur_y, &txt[start], end-start);
        cur_x += width;
        lasthyph = hashyph;
    }

    mouth.rewind();
    mouth.render();
    game_frame();
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
    BigAnimation mouth(filename, false);

    Dialog dlg(charname);
    dlg.load(dlgname);

    sprintf(filename, "chars/%s/%s.vb", charname, dlgname);
    Slice vars = try_read_xored(filename);

    SavedScreen clean_scr;

    int state = dlg.get_root();
    while (state) {
        clean_scr.restore(); // reset everything to start
        
        const DialogString *str;
        state = dlg.decode_and_follow(state, str);

        say_line(mouth, str->text, str->text_len);

        // render the dialog options
        int cur_y = 144;
        for (int i=0; i < 5; i++) {
            int option = dlg.get_next(state, i);
            if (!option)
                break;

            str = dlg.decode(option);
            if (!str)
                break;

            // TODO line breaking!
            bigfont.print(0, cur_y, (const char*)str->text, str->text_len);
            cur_y += 10;
        }

        // wait for response
        for (;;)
            game_frame();
        break;
    }
}