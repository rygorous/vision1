#include "script.h"
#include "graphics.h"
#include "main.h"
#include "util.h"
#include "str.h"
#include "vars.h"
#include "dialog.h"
#include "mouse.h"
#include "font.h"
#include "corridor.h"
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <vector>

// ---- game tick

namespace {
    struct AnimDesc {
        Animation *anim;
        PixelSlice target;
        bool is_looped;
    };
}

static std::vector<AnimDesc> animations;

static void clear_anim()
{
    while (animations.size()) {
        delete animations.back().anim;
        animations.pop_back();
    }
}

static void add_anim(Animation *anim, bool looped, PixelSlice target)
{
    AnimDesc desc;
    desc.anim = anim;
    desc.target = target;
    desc.is_looped = looped;
    animations.push_back(desc);
}

static void render_anim()
{
    for (auto it = animations.begin(); it != animations.end(); ++it)
        it->anim->render(it->target);
}

static void tick_anim()
{
    for (size_t i = 0; i < animations.size(); ) {
        AnimDesc &desc = animations[i];
        desc.anim->tick();
        if (desc.anim->is_done()) {
            delete desc.anim;
            animations[i] = animations.back();
            animations.pop_back();
        } else
            i++;
    }
}

static bool are_anims_done()
{
    for (auto it = animations.begin(); it != animations.end(); ++it)
        if (!it->is_looped)
            return false;
    return true;
}

static void wait_anim_done()
{
    while (!are_anims_done())
        game_frame();
}

// ---- scroll window

static void print_clear();

static const int SCROLL_SCREEN_WIDTH = 320;
static const int SCROLL_WINDOW_WIDTH = SCROLL_SCREEN_WIDTH*4;
static const int SCROLL_WINDOW_Y0 = 32;
static const int SCROLL_WINDOW_Y1 = 144;
static const int SCROLL_WINDOW_HEIGHT = SCROLL_WINDOW_Y1 - SCROLL_WINDOW_Y0;

static PixelSlice scroll_window;
static int scroll_x, scroll_x_min, scroll_x_max;
static bool scroll_auto;

static void scroll_disable()
{
    scroll_window = PixelSlice();
    scroll_x = 0;
    scroll_x_min = scroll_x_max = 0;
}

static void scroll_enable()
{
    if (scroll_window)
        return;

    scroll_window = PixelSlice::make(SCROLL_WINDOW_WIDTH, vga_screen.height());
}

static void scroll_tick()
{
    static const int scroll_border = 32;
    if (!scroll_auto)
        return;

    int new_x = scroll_x;
    if (mouse_x < scroll_border)
        new_x = std::max(scroll_x - 1, scroll_x_min);
    else if (mouse_x >= vga_screen.width() - scroll_border)
        new_x = std::min(scroll_x + 1, scroll_x_max);

    if (new_x != scroll_x) {
        print_clear();
        scroll_x = new_x;
    }
}

static PixelSlice scroll_getscreen()
{
    if (scroll_window)
        return scroll_window.slice(scroll_x, 0, scroll_x + vga_screen.width(), vga_screen.height());
    else
        return vga_screen;
}

// ---- text printing

static PixelSlice print_saveunder;
static int print_savey;

static void print_clear()
{
    if (!print_saveunder)
        return;

    blit(scroll_getscreen(), 0, print_savey, print_saveunder);
    print_saveunder = PixelSlice();
}

static int print_getlinelen(const char *str)
{
    const char *hash = strchr(str, '#');
    return hash ? (int) (hash - str) : strlen(str);
}

static void print_getsize(int *w, int *h, const char *str)
{
    *w = 0;
    *h = 0;
    while (*str) {
        int len = print_getlinelen(str);
        *w = std::max(*w, bigfont->str_width(str, len));
        *h += 10;

        str += len;
        if (*str == '#')
            str++;
    }
}

static void print_text(const char *str)
{
    print_clear();

    int w, h;
    print_getsize(&w, &h, str);

    PixelSlice screen = scroll_getscreen();
    print_savey = 126 - h/2;
    print_saveunder = screen.slice(0, print_savey, vga_screen.width(), print_savey + h).clone();

    int y = print_savey;
    while (*str) {
        int len = print_getlinelen(str);
        int w = bigfont->str_width(str, len);
        bigfont->print(screen, (320 - w) / 2, y, str, len);

        y += 10;
        str += len;
        if (*str == '#')
            str++;
    }
}

// ---- hot spots

static PixelSlice hotspots;

static void hotspot_load(const char *filename, int screen)
{
    PixelSlice img = load_hot(read_file(filename));
    img = img.slice(0, SCROLL_WINDOW_Y0/2, img.width(), SCROLL_WINDOW_Y1/2);

    // save it!
    if (screen == 0)
        hotspots = img.clone();
    else {
        if (hotspots.width() != SCROLL_WINDOW_WIDTH / 2)
            hotspots = PixelSlice::black(SCROLL_WINDOW_WIDTH / 2, img.height());

        blit(hotspots, (screen - 1) * (SCROLL_SCREEN_WIDTH / 2), 0, img);
    }
}

static void hotspot_reset()
{
    hotspots = PixelSlice();
}

static int hotspot_get(int mouse_x, int mouse_y)
{
    int hot_x = (mouse_x + scroll_x) / 2;
    int hot_y = (mouse_y - SCROLL_WINDOW_Y0) / 2;
    if (hot_x >= 0 && hot_y >= 0 && hot_x < hotspots.width() && hot_y < hotspots.height())
        return *hotspots.ptr(hot_x, hot_y);
    else
        return 0;
}

static void hotspot_kill(int which)
{
    for (int y=0; y < hotspots.height(); y++) {
        U8 *p = hotspots.row(y);
        for (int x=0; x < hotspots.width(); x++)
            if (p[x] == which)
                p[x] = 0;
    }
}

// ---- cursor handling

static MouseCursor hot2cursor[256];

static void cursor_reset()
{
    hot2cursor[0] = MC_NORMAL;
    for (int i=1; i < ARRAY_COUNT(hot2cursor); i++)
        hot2cursor[i] = MC_NULL;
}

static void cursor_define(int base, const char *code)
{
    for (int i=0; code[i]; i++) {
        int which = base + i;
        if (which >= 1 && which < ARRAY_COUNT(hot2cursor))
            hot2cursor[which] = get_mouse_cursor_from_char(code[i]);
    }
}

// ---- script low-level scanning

static Slice source, scan, line;
static bool isInit;
static int flow_counter;
static int nest_counter;

static int hotspot_clicked = 0;
static int hotspot_last = 0;
static int hotspot_counter = 0;
static MouseCursor cursor_override;
static char cursors[256];

static void scan_line()
{
    line = chop_line(scan);
}

static void skip_whitespace()
{
    line = eat_heading_space(line);
    // an end-of-line comment starts with ; and counts as white space
    if (line.len() && line[0] == ';')
        line = line(line.len());
}

static Slice scan_word()
{
    U32 pos = 0;
    while (pos < line.len() && line[pos] != ' ' && line[pos] != ';')
        pos++;

    Slice s = line(0, pos);
    line = line(pos);
    skip_whitespace();
    return s;
}

// ---- debug

static void print(Slice s)
{
    U32 pos = 0;
    while (pos < s.len()) {
        putc(s[pos], stdout);
        pos++;
    }
}

// ---- "higher-level" parsing

static bool int_literal(const Slice &s, int &i)
{
    U32 pos = 0;
    int sign = 1;

    i = 0;
    if (pos < s.len() && s[pos] == '-')
        sign = -1, pos++;

    while (pos < s.len() && isdigit(s[pos]))
        i = (i * 10) + (s[pos++] - '0');

    i *= sign;
    return pos == s.len();
}

static int need_int_literal(const Slice &s)
{
    int i;
    if (!int_literal(s, i))
        panic("int literal expected: \"%s\"", to_string(s).c_str());
    return i;
}

static int int_value(const Slice &value)
{
    // either a variable (which gets evaluated) or a literal
    int i;
    if (int_literal(value, i)) // parses as int?
        return i;
    else // assume it's a variable name
        return get_var_int(to_string(value));
}

static int int_value_word()
{
    return int_value(scan_word());
}

static Str str_value(const Slice &value)
{
    if (value[0] == '\'') { // literal
        if (value[value.len()-1] != '\'')
            panic("bad string literal!");

        return to_string(value(1, value.len()-1));
    } else
        return get_var_as_str(to_string(value));
}

static Str str_value_word()
{
    if (line.len() && line[0] == '\'') {
        // find matching '
        U32 pos = 1;
        while (pos < line.len() && line[pos] != '\'')
            pos++;
        if (pos < line.len())
            pos++;
        Str s = str_value(line(0, pos));
        line = line(pos);
        return s;
    } else
        return str_value(scan_word());
}

static Str str_word()
{
    return to_string(scan_word());
}

static bool has_prefixi(const Slice &value, const char *str)
{
    U32 pos = 0;
    while (pos < value.len() && str[pos] && tolower(value[pos]) == str[pos])
        pos++;
    return str[pos] == 0;
}

static bool is_equal(const Slice &value, const char *str)
{
    return has_prefixi(value, str) && str[value.len()] == 0;
}

static bool is_equal(const Slice &a, const Slice &b)
{
    if (a.len() != b.len())
        return false;

    for (U32 pos=0; pos < a.len(); pos++)
        if (tolower(a[pos]) != tolower(b[pos]))
            return false;

    return true;
}

// ---- boolean expressions

static bool is_bool_expr_op(char ch)
{
    return ch == '^' || ch == '<' || ch == '=' || ch == '>' || ch == '#';
}

static Slice scan_bool_tok()
{
    U32 pos = 0;

    if (line.len() && line[0] == '\'') { // string
        pos++;
        while (pos < line.len() && line[pos] != '\'')
            pos++;

        if (pos == line.len())
            panic("string continued past end of line");
        else
            pos++; // quote is part of the string!
    } else {
        while (pos < line.len() && line[pos] != ' ' && line[pos] != ';' && !is_bool_expr_op(line[pos]))
            pos++;
    }

    // handle single-character tokens
    if (pos == 0 && line.len() && is_bool_expr_op(line[0]))
        pos = 1;

    Slice s = line(0, pos);
    line = line(pos);
    skip_whitespace();
    return s;
}

static Str bool_str_value(const Slice &value)
{
    if (value[0] == '\'') { // quoted literal
       if (value[value.len()-1] != '\'')
            panic("bad string literal!");

        return to_string(value(1, value.len()-1));
    } else if (value[value.len()-1] == '$') // string variable
        return get_var_str(to_string(value).c_str());
    else
        return to_string(value);
}

static bool eval_bool_expr1(Slice &tok)
{
    if (!tok.len())
        return true;

    if (tok[0] == '\'' || tok[tok.len() - 1] == '$') { // string compare
        Str lhs = bool_str_value(tok);
        Slice op = scan_bool_tok();
        Str rhs = bool_str_value(scan_bool_tok());
        tok = scan_bool_tok();

        if (is_equal(op, "="))
            return lhs == rhs;
        else if (is_equal(op, "#"))
            return lhs != rhs;
        else
            panic("unknown relational op (str): %s", to_string(op).c_str());
    } else { // assume integers
        int lhs = int_value(tok);
        Slice op = scan_bool_tok();
        if (op.len()) {
            int rhs = int_value(scan_bool_tok());
            tok = scan_bool_tok();

            if (is_equal(op, "="))
                return lhs == rhs;
            else if (is_equal(op, "#"))
                return lhs != rhs;
            else if (is_equal(op, "<"))
                return lhs < rhs;
            else if (is_equal(op, ">"))
                return lhs > rhs;
            else
                panic("unknown relational op (int): %s", to_string(op).c_str());
        } else { // just a single variable
            tok = op;
            return lhs != 0;
        }
    }

    return false;
}

static bool eval_bool_expr0(Slice &tok)
{
    enum {
        LSET,
        LOR,
        LAND,
        LXOR
    } logic = LSET;
    bool result = false;

    for (;;) {
        bool partial;
        bool complement = false;

        if (is_equal(tok, "not")) {
            complement = true;
            tok = scan_bool_tok();
        }

        if (logic != LSET && tok.len() && tok[0] >= '0' && tok[0] <= '9') {
            // special case: implicit PERSO=
            partial = get_var_int("PERSO") == int_value(tok);
            tok = scan_bool_tok();
        } else
            partial = eval_bool_expr1(tok);

        switch (logic) {
        case LSET:  result = partial; break;
        case LOR:   result |= partial; break;
        case LAND:  result &= partial; break;
        case LXOR:  result ^= partial; break;
        }

        if (is_equal(tok, "or")) {
            logic = LOR;
            tok = scan_bool_tok();
        } else if (is_equal(tok, "and")) {
            logic = LAND;
            tok = scan_bool_tok();
        } else if (is_equal(tok, "xor")) {
            logic = LXOR;
            tok = scan_bool_tok();
        } else if (!tok.len() || is_equal(tok, "^"))
            break;
        else
            panic("unknown boolean op: %s", to_string(tok).c_str());
    }

    return result;
}

bool eval_bool_expr(const Slice &expr)
{
    Slice tok;

    line = expr;
    skip_whitespace();
    tok = scan_bool_tok(); // 1 token lookahead

    // handle caret (shortcut eval or)
    while (tok.len()) {
        if (eval_bool_expr0(tok))
            return true;

        if (!is_equal(tok, "^"))
            break;
        else
            tok = scan_bool_tok();
    }

    return false;
}

static bool bool_expr()
{
    return eval_bool_expr(line);
}

// ---- command functions

static void cmd_if()
{
    nest_counter++;

    if (flow_counter == 0) { // normal execution
        bool cond = false;
        Slice l = line;

        // special cases first
        // TODO there's more of them!
        if (has_prefixi(line, "init"))
            cond = isInit;
        else if (has_prefixi(line, "hot"))
            cond = need_int_literal(line(3)) == hotspot_clicked;
        else if (has_prefixi(line, "cnt"))
            cond = need_int_literal(line(3)) == hotspot_counter;
        else if (has_prefixi(line, "key"))
            cond = false; // TODO real impl!
        else // assume it's an expression
            cond = bool_expr();

        flow_counter = cond ? 0 : 1;
        //printf("if: expr=\"%s\" value=%d\n", to_string(l).c_str(), cond);
    } else { // already not executing, just increment nesting depth
        //printf("if: expr=\"%s\" flow=%d\n", to_string(line).c_str(), flow_counter);
        flow_counter++;
    }
}

static void cmd_else()
{
    if (flow_counter <= 1)
        flow_counter = !flow_counter;
}

static void cmd_end()
{
    nest_counter--;
    if (flow_counter != 0)
        flow_counter--;
}

static void cmd_set()
{
    Str varname = str_word();
    if (varname.back() == '$')
        set_var_str(varname, str_value_word());
    else
        set_var_int(varname, int_value_word());
}

static void cmd_add()
{
    Str varname = str_word();
    if (varname.back() == '$')
        set_var_str(varname, get_var_str(varname) + str_value_word());
    else
        set_var_int(varname, get_var_int(varname) + int_value_word());
}

static void cmd_sub()
{
    Str varname = str_word();
    set_var_int(varname, get_var_int(varname) - int_value_word());
}

static void cmd_off()
{
    // turn off music?
    //assert(0);
    //printf("off?\n");
}

static void cmd_pic()
{
    clear_anim();

    Str filename = str_word();
    load_background(filename.c_str());

    Slice other = scan_word();
    if (is_equal(other, "b")) {
        memcpy(palette_b, palette_a, sizeof(Palette));
        set_palette();
    }
    else
        set_palette();
}

static void cmd_keyenable()
{
    int flag = int_value_word();
    printf("keyena: set to %d\n", flag);
}

static void cmd_song()
{
    // just ignored for now
}

static void cmd_fade()
{
    Slice dir = scan_word();
    int duration = int_value_word();
    duration = duration * 7; // is in tenths of seconds, want 70fps steps

    if (is_equal(dir, "in")) {
        for (int i=1; i <= duration; i++) {
            set_palb_fade(256 * i / duration);
            frame();
        }
    } else if (is_equal(dir, "out")) {
        for (int i=duration-1; i >= 0; i--) {
            set_palb_fade(256 * i / duration);
            frame();
        }
    } else
        panic("unknown fade direction");
}

static void cmd_exec()
{
    Slice what = scan_word();
    if (is_equal(what, "dialog")) {
        Str charname = str_word();
        Str dlgname = str_word();

        scroll_disable();
        run_dialog(charname.c_str(), dlgname.c_str());
    } else {
        printf("don't know how to exec: ");
        print(what);
        printf("\n");
    }
}

static void cmd_random()
{
    Str varname = str_word();
    int range = int_value_word();
    if (!range)
        range = 1;
    set_var_int(varname, rand() % range);
}

static void cmd_wait()
{
    // TODO wait has optional para, what does it do?
    wait_anim_done();
}

static void cmd_jump()
{
    Slice label = line;
    scan = source;

    printf("exec JUMP to '%s'\n", to_string(label).c_str());

    while (scan.len()) {
        scan_line();
        skip_whitespace();
        if (line.len() && line[0] == ':' && is_equal(line(1), label))
            return; // found our label and scan is in the right place
    }

    panic("label '%s' not found in script!\n", to_string(label).c_str());
}

static void cmd_definelabel()
{
    // no semantics
}

static void cmd_stop()
{
    exit(1); // TODO this is not exactly a nice way to do it!
}

static void cmd_time()
{
    // TODO set in-game timer!
    //assert(0);
    printf("TIME %s\n", to_string(line).c_str());
}

static void cmd_load()
{
    game_command(to_string(line).c_str());
}

static void cmd_black()
{
    solid_fill(vga_screen, 0);
    memset(&vga_pal, 0, sizeof(vga_pal));
}

static void cmd_big()
{
    int flags = 0;
    Str filename = str_word() + ".ani";
    if (filename[0] == '!') {
        flags |= BA_REVERSE;
        filename = filename.substr(1);
    }

    assert(!scroll_window);
    add_anim(new_big_anim(filename.c_str(), flags), false, vga_screen);
}

static void cmd_megaanim()
{
    Str grafilename = str_word();
    Str prefix = str_word();
    int frame_start = int_value_word();
    int frame_end = int_value_word();
    int posx = int_value_word();
    int posy = int_value_word();
    int delay = int_value_word();
    int scale = int_value_word();
    int flip = int_value_word();

    assert(!scroll_window);
    add_anim(new_mega_anim(grafilename.c_str(), prefix.c_str(), frame_start, frame_end, posx, posy,
        delay, scale, flip), false, vga_screen);
}

static void cmd_color()
{
    int index = int_value_word();
    int r = int_value_word();
    int g = int_value_word();
    int b = int_value_word();

    // TODO this should modify graphics pal!
    if (index >= 0 && index < 256) {
        vga_pal[index].r = r;
        vga_pal[index].g = g;
        vga_pal[index].b = b;
    }
}

static void cmd_cycle()
{
    int first = int_value_word();
    int last = int_value_word();
    int delay = int_value_word();
    int dir = int_value_word();

    add_anim(new_color_cycle_anim(first, last, delay, dir), true, vga_screen);
}

static void cmd_fx()
{
    // TODO sound
}

static void cmd_grafix()
{
    // TODO grafix
}

static void cmd_def()
{
    int which = int_value_word();
    Str code = str_word();
    cursor_define(which, code.c_str());
}

static void cmd_hot()
{
    Str filename = str_word();
    int screen = int_value_word();

    hotspot_load(filename.c_str(), screen);
}

static void cmd_print()
{
    print_text(to_string(line).c_str());
}

static void cmd_ani()
{
    int flags = BA_PING_PONG;
    Str filename = str_word() + ".ani";
    int screen = int_value_word();

    if (filename[0] == '!') {
        flags |= BA_REVERSE;
        filename = filename.substr(1);
    }

    PixelSlice target = vga_screen;
    if (screen != 0) {
        assert(scroll_window != 0);
        int x0 = (screen - 1) * SCROLL_SCREEN_WIDTH;
        target = scroll_window.slice(x0, 0, x0 + SCROLL_SCREEN_WIDTH, vga_screen.height());
    }

    add_anim(new_big_anim(filename.c_str(), flags), true, target);
}

static void cmd_back()
{
    Str filename = str_word();
    int slot = int_value_word();

    load_background(filename.c_str());
    set_palette();

    if (slot != 0) {
        scroll_enable();
        blit(scroll_window, (slot - 1) * SCROLL_SCREEN_WIDTH, 0, vga_screen);
    }
}

static void cmd_scroll()
{
    Str mode = str_word();
    scroll_x_min = int_value_word();
    scroll_x_max = int_value_word();
    scroll_auto = tolower(mode) == "auto";
    printf("scroll: mode=%s min=%d max=%d\n", mode.c_str(), scroll_x_min, scroll_x_max);
}

static void cmd_start()
{
    scroll_x = int_value_word();
}

static void cmd_pointer()
{
    Str which = str_word();
    if (!which.empty())
        cursor_override = get_mouse_cursor_from_char(which[0]);
}

static void cmd_killhotspot()
{
    int which = int_value_word();
    hotspot_kill(which);
    printf("KILLHOTSPOT %d\n", which);
}

static void cmd_write()
{
	Slice xw = scan_word();
	int x = 0;
	int y = int_value_word();
	Str text = to_string(line);

	if (xw.len() && isdigit(xw[0]))
		x = int_value(xw);
	else if (to_string(xw) == "c") // center
		x = (vga_screen.width() - bigfont->str_width(text.c_str())) / 2;

	bigfont->print(scroll_getscreen(), x, y, text.c_str());
}

static void cmd_x0()
{
	printf("X0?\n");
}

static void cmd_x1()
{
	printf("X1?\n");
}

static struct CommandDesc
{
    char *name;
    int prefixlen;
    bool isflow;
    void (*exec)();
} commands[] = {
    ":",            1,  false,  cmd_definelabel,
    "add",          2,  false,  cmd_add,
    "ani",          2,  false,  cmd_ani,
    "back",         2,  false,  cmd_back,
    "big",          2,  false,  cmd_big,
    "black",        2,  false,  cmd_black,
    "color",        2,  false,  cmd_color,
    "cycle",        2,  false,  cmd_cycle,
    "def",          2,  false,  cmd_def,
    "else",         2,  true,   cmd_else,
    "end",          2,  true,   cmd_end,
    "exec",         2,  false,  cmd_exec,
    "fade",         2,  false,  cmd_fade,
    "fx",           2,  false,  cmd_fx,
    "grafix",       2,  false,  cmd_grafix,
    "hot",          2,  false,  cmd_hot,
    "if",           2,  true,   cmd_if,
    "jump",         1,  false,  cmd_jump,
    "keyenable",    2,  false,  cmd_keyenable,
    "killhotspot",  2,  false,  cmd_killhotspot,
    "load",         2,  false,  cmd_load,
    "megaani",      2,  false,  cmd_megaanim,
    "off",          2,  false,  cmd_off,
    "pic",          2,  false,  cmd_pic,
    "pointer",      2,  false,  cmd_pointer,
    "print",        2,  false,  cmd_print,
    "scroll",       2,  false,  cmd_scroll,
    "set",          2,  false,  cmd_set,
    "song",         2,  false,  cmd_song,
    "start",        3,  false,  cmd_start,
    "stop",         3,  false,  cmd_stop,
    "sub",          2,  false,  cmd_sub,
    "time",         2,  false,  cmd_time,
    "random",       2,  false,  cmd_random,
    "wait",         2,  false,  cmd_wait,
	"write",		2,	false,	cmd_write,
	"x0",			2,	false,	cmd_x0,
	"x1",			2,	false,	cmd_x1,
};

static void run_script(Slice code, bool init)
{
    // init scan
    source = code;
    scan = code;
    isInit = init;
    flow_counter = 0;
    nest_counter = 0;

    // scan script
    while (scan.len()) {
        assert(flow_counter >= 0);

        scan_line();
        skip_whitespace();

        Slice orig_line = line;
        //if (flow_counter == 0)
        //    printf("sc: '%s'\n", to_string(line).c_str());

        Slice command = scan_word();

        int noffs = 0;
        if (has_prefixi(command, "el") || has_prefixi(command, "en"))
            noffs = -1;
        //printf("%c%*s%s\n", flow_counter ? '-' : ' ', 2*(nest_counter+noffs), "", to_string(orig_line).c_str());

        int i;
        for (i=0; i < ARRAY_COUNT(commands); i++) {
            int j=0;
            while (j < commands[i].prefixlen && tolower(command[j]) == commands[i].name[j])
                j++;
            if (j == commands[i].prefixlen) {
                if (flow_counter == 0 || commands[i].isflow)
                    commands[i].exec();
                break;
            }
        }

        if (i == ARRAY_COUNT(commands)) {
            print(command);
            printf("? (line=\"%s\")\n", to_string(line).c_str());
            //assert(0);
        }
    }
}

// ---- outer logic

enum GameMode
{
    GM_ROOM,
    GM_CORRIDOR
};

static Slice s_script;
static Str s_command;
static GameMode s_mode = GM_ROOM;

void game_command(const char *cmd)
{
    assert(s_command.empty());
    s_command = cmd;
}

void game_frame()
{
    render_anim();
    tick_anim();

    // time handling etc. should also go here

    frame();
}

static void game_reset()
{
    clear_anim();
    scroll_disable();
    hotspot_reset();
    cursor_reset();
    print_clear();
    hotspot_clicked = 0;
}

static void handle_hot_click(int hot)
{
	hotspot_clicked = hot;
	if (!hot)
		return;

	if (hot != hotspot_last)
		hotspot_counter = 1;
	else
		hotspot_counter++;
	hotspot_last = hot;
	cursor_override = MC_NULL;
}

static void game_script_tick_room()
{
    int hot = hotspot_get(mouse_x, mouse_y);
    MouseCursor cursor = hot2cursor[hot];
    if (hot == hotspot_last && cursor_override)
        cursor = cursor_override;

    set_mouse_cursor(cursor);

#if 0 // hotspot debug
    PixelSlice target = scroll_getscreen();
    for (int y=SCROLL_WINDOW_Y0; y < SCROLL_WINDOW_Y1; y++) {
        for (int x=0; x < 320; x++) {
            int hot = hotspot_get(x, y);
            if (hot)
                *target.ptr(x, y) = 1;
        }
    }
#endif

    scroll_tick();

    static int old_button;
    int button_down = mouse_button & ~old_button;
    old_button = mouse_button;

    hotspot_clicked = 0;
    if (button_down) {
        print_clear();
		handle_hot_click(hotspot_get(mouse_x, mouse_y));

        run_script(s_script, false);
    }
}

static void game_script_tick_corridor()
{
    int hot = hotspot_get(mouse_x, mouse_y);
    MouseCursor cursor = hot2cursor[hot];
    if (hot == hotspot_last && cursor_override)
        cursor = cursor_override;

    set_mouse_cursor(cursor);

    static int old_button;
    int button_down = mouse_button & ~old_button;
    old_button = mouse_button;

	hotspot_clicked = 0;
    if (button_down && hot) {
		handle_hot_click(hot);
		corridor_click(hot);
	}
}

void game_script_tick()
{
    if (!s_command.empty()) {
        game_reset();

        if (has_prefixi(s_command, "welt ")) {
            Str filename = "data/" + s_command.substr(5) + ".par";
            s_command = "";

            s_mode = GM_ROOM;
            s_script = read_xored(filename.c_str());
            run_script(s_script, true);
        } else if (has_prefixi(s_command, "gang ")) {
            // command parsing?
            s_command = "";

            s_mode = GM_CORRIDOR;
            hotspots = PixelSlice::black(SCROLL_SCREEN_WIDTH / 2, SCROLL_WINDOW_HEIGHT / 2);

            cursor_define(1, "urvl");
            corridor_start();
            corridor_render();
        } else
            panic("bad game command: \"%s\"", s_command.c_str());
    } else {
        switch (s_mode) {
        case GM_ROOM:       game_script_tick_room(); break;
        case GM_CORRIDOR:   game_script_tick_corridor(); break;
        default:            panic("bad state"); break;
        }
    }
}

void game_script_run(const Slice &script)
{
	assert(s_mode != GM_ROOM);
	printf("running script:\n-\n%s\n-\n", to_string(script).c_str());
	run_script(script, false);
}

void game_shutdown()
{
    game_reset();
}

const U8 *game_get_screen_row(int y)
{
    if (y < 0 || y >= 200)
        return 0;

    if (scroll_window && y >= SCROLL_WINDOW_Y0 && y < SCROLL_WINDOW_Y1)
        return scroll_window.ptr(scroll_x, y);

    return vga_screen.row(y);
}

PixelSlice &game_get_hotspots()
{
	return hotspots;
}

void game_hotspot_define(int which, char code)
{
	char str[2] = { code, 0 };
	cursor_define(which, str);
}

void game_hotspot_define_multi(int which, const char *codes)
{
	cursor_define(which, codes);
}

void game_hotspot_disable(int which)
{
    hotspot_kill(which);
}
