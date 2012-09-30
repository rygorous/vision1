#include "script.h"
#include "graphics.h"
#include "main.h"
#include "util.h"
#include "vars.h"
#include "dialog.h"
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <vector>

// ---- game tick

namespace {
    struct AnimDesc {
        Animation *anim;
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

static void add_anim(Animation *anim, bool looped=false)
{
    AnimDesc desc;
    desc.anim = anim;
    desc.is_looped = looped;
    animations.push_back(desc);
}

static void render_anim()
{
    for (auto it = animations.begin(); it != animations.end(); ++it)
        it->anim->render();
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

// ---- script low-level scanning

static Slice source, scan, line;
static bool isInit;
static int flow_counter;
static int nest_counter;

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
        error_exit("int literal expected: \"%s\"", to_string(s).c_str());
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

static std::string str_value(const Slice &value)
{
    if (value[0] == '\'') { // literal
        if (value[value.len()-1] != '\'')
            error_exit("bad string literal!");

        return to_string(value(1, value.len()-1));
    } else
        return get_var_as_str(to_string(value));
}

static std::string str_value_word()
{
    if (line.len() && line[0] == '\'') {
        // find matching '
        U32 pos = 1;
        while (pos < line.len() && line[pos] != '\'')
            pos++;
        if (pos < line.len())
            pos++;
        std::string s = str_value(line(0, pos));
        line = line(pos);
        return s;
    } else
        return str_value(scan_word());
}

static std::string str_word()
{
    return to_string(scan_word());
}

static bool has_prefix(const std::string &value, const char *str)
{
    size_t pos = 0;
    while (pos < value.size() && str[pos] && tolower(value[pos]) == str[pos])
        pos++;
    return str[pos] == 0;
}

static bool has_prefix(const Slice &value, const char *str)
{
    U32 pos = 0;
    while (pos < value.len() && str[pos] && tolower(value[pos]) == str[pos])
        pos++;
    return str[pos] == 0;
}

static bool is_equal(const Slice &value, const char *str)
{
    return has_prefix(value, str) && str[value.len()] == 0;
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
            error_exit("string continued past end of line");
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

static std::string bool_str_value(const Slice &value)
{
    if (value[0] == '\'') { // quoted literal
       if (value[value.len()-1] != '\'')
            error_exit("bad string literal!");

        return to_string(value(1, value.len()-1));
    } else if (value[value.len()-1] == '$') // string variable
        return get_var_str(to_string(value));
    else
        return to_string(value);
}

static bool eval_bool_expr1(Slice &tok)
{
    if (!tok.len())
        return true;

    if (tok[0] == '\'' || tok[tok.len() - 1] == '$') { // string compare
        std::string lhs = bool_str_value(tok);
        Slice op = scan_bool_tok();
        std::string rhs = bool_str_value(scan_bool_tok());
        tok = scan_bool_tok();

        if (is_equal(op, "="))
            return lhs == rhs;
        else if (is_equal(op, "#"))
            return lhs != rhs;
        else
            error_exit("unknown relational op (str): %s", to_string(op).c_str());
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
                error_exit("unknown relational op (int): %s", to_string(op).c_str());
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
            error_exit("unknown boolean op: %s", to_string(tok).c_str());
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
        if (has_prefix(line, "init"))
            cond = isInit;
        else if (has_prefix(line, "hot"))
            cond = need_int_literal(line(3)) == -1; // TODO real impl!
        else if (has_prefix(line, "cnt"))
            cond = need_int_literal(line(3)) == 1234; // TODO real impl!
        else if (has_prefix(line, "key"))
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
    std::string varname = str_word();
    if (varname.back() == '$')
        set_var_str(varname, str_value_word());
    else
        set_var_int(varname, int_value_word());
}

static void cmd_add()
{
    std::string varname = str_word();
    if (varname.back() == '$')
        set_var_str(varname, get_var_str(varname) + str_value_word());
    else
        set_var_int(varname, get_var_int(varname) + int_value_word());
}

static void cmd_sub()
{
    std::string varname = str_word();
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

    std::string filename = str_word();
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
        error_exit("unknown fade direction");
}

static void cmd_exec()
{
    Slice what = scan_word();
    if (is_equal(what, "dialog")) {
        std::string charname = str_word();
        std::string dlgname = str_word();

        run_dialog(charname.c_str(), dlgname.c_str());
    } else {
        printf("don't know how to exec: ");
        print(what);
        printf("\n");
    }
}

static void cmd_random()
{
    std::string varname = str_word();
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

    error_exit("label '%s' not found in script!\n", to_string(label).c_str());
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
    memset(vga_screen, 0, sizeof(vga_screen));
    memset(&vga_pal, 0, sizeof(vga_pal));
}

static void cmd_big()
{
    int flags = 0;
    std::string filename = str_word() + ".ani";
    if (filename[0] == '!') {
        flags |= BA_REVERSE;
        filename = filename.substr(1);
    }
    
    add_anim(new_big_anim(filename.c_str(), flags));
}

static void cmd_megaanim()
{
    std::string grafilename = str_word();
    std::string prefix = str_word();
    int frame_start = int_value_word();
    int frame_end = int_value_word();
    int posx = int_value_word();
    int posy = int_value_word();
    int delay = int_value_word();
    int scale = int_value_word();
    int flip = int_value_word();

    add_anim(new_mega_anim(grafilename.c_str(), prefix.c_str(), frame_start, frame_end, posx, posy,
        delay, scale, flip));
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

    add_anim(new_color_cycle_anim(first, last, delay, dir), true);
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
    printf("DEF %s\n", to_string(line).c_str());
}

static void cmd_hot()
{
    printf("HOT %s\n", to_string(line).c_str());
}

static void cmd_print()
{
    printf("PRINT %s\n", to_string(line).c_str());
}

static void cmd_ani()
{
    int flags = BA_PING_PONG;
    std::string filename = str_word() + ".ani";
    int screen = int_value_word();

    if (filename[0] == '!') {
        flags |= BA_REVERSE;
        filename = filename.substr(1);
    }
    
    if (screen == 1) // TODO proper scroll support
        add_anim(new_big_anim(filename.c_str(), flags), true);
}

static void cmd_back()
{
    std::string filename = str_word();
    int slot = int_value_word();

    if (slot == 1) { // TODO proper scroll support
        load_background(filename.c_str());
        set_palette();
    }
}

static void cmd_scroll()
{
    printf("SCROLL %s\n", to_string(line).c_str());
}

static void cmd_start()
{
    printf("START %s\n", to_string(line).c_str());
}

static void cmd_pointer()
{
    printf("POINTER %s\n", to_string(line).c_str());
}

static void cmd_killhotspot()
{
    int which = int_value_word();
    printf("KILLHOTSPOT %d\n", which);
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
        if (has_prefix(command, "el") || has_prefix(command, "en"))
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

static Slice s_script;
static std::string s_command;

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

void game_script_tick()
{
    if (!s_command.empty()) {
        if (has_prefix(s_command, "welt ")) {
            std::string filename = "data/" + s_command.substr(5) + ".par";
            s_command.clear();

            s_script = read_xored(filename.c_str());
            run_script(s_script, true);
        } else
            error_exit("bad game command: \"%s\"", s_command);
    } else
        run_script(s_script, false);
}

void game_shutdown()
{
    clear_anim();
}

const U8 *game_get_screen_row(int y)
{
    return vga_screen.row(y);
}
