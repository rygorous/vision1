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

        const DialogString *decode(int item) const;
        int get_next(int item, int which) const;
        int find_label_rec(int item, const U8 label[LABEL_LEN]) const;
        int find_label(const char *which, const char *whichend) const;

    public:
        Dialog(const char *charname);

        void load(const char *dlgname);
        int get_root() const;
        int decode_and_follow(int state, const DialogString *&str);
    };
}

const DialogString *Dialog::decode(int item) const
{
    int offs = dir.at(item + 7);
    return (offs == 0xffff) ? nullptr : (const DialogString *)&data[offs];
}

int Dialog::get_next(int item, int which) const
{
    if (which < 0 || which > 5)
        return 0;
    else
        return dir[item + 1 + which];
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

    // use top 128 palette entries from .frz?
    memcpy(&palette_a[128], &s[128*sizeof(PalEntry)], 128*sizeof(PalEntry));
    memcpy(&palette_b[128], &s[128*sizeof(PalEntry)], 128*sizeof(PalEntry));
    decode_delta(vga_screen, &s[256*sizeof(PalEntry)]);
    set_palette();
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
    BigAnimation talk(filename, false);

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

        // say the lines
        char text[256];
        memcpy(text, str->text, str->text_len);
        text[str->text_len] = 0;
        printf("%s\n", text);

        // get response
        break;
    }

    /*for (int i=0; i < 70; i++) {
        talk.render();
        talk.tick();
        if (talk.is_done())
            talk.rewind();
        game_frame();
    }*/
}