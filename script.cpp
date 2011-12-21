#include "common.h"
#include <stdio.h>
#include <ctype.h>

static std::string to_string(const Slice &sl)
{
    Slice &s = (Slice &)sl;
    return std::string(&s[0], &s[0] + s.len());
}

static Slice scan, line;
static bool isInit;

static bool islinespace(U8 ch)
{
    return ch == '\r' || ch == '\n';
}

static void scan_line()
{
    U32 pos = 0;

    // find end of current line
    pos = 0;
    while (pos < scan.len() && !islinespace(scan[pos]))
        pos++;
    line = scan(0, pos);

    // forward scan to start of next non-empty line
    while (pos < scan.len() && islinespace(scan[pos]))
        pos++;
    scan = scan(pos);
}

static void skip_whitespace()
{
    U32 pos = 0;
    while (pos < line.len() && line[pos] == ' ')
        pos++;
    line = line(pos);
}

static Slice scan_word()
{
    U32 pos = 0;
    while (pos < line.len() && line[pos] != ' ')
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

static int int_value(const Slice &value)
{
    // either a variable (which gets evaluated) or a literal
    U32 pos = 0;
    int i = 0, sign = 1;

    // try to parse as an int
    if (pos < value.len() && value[pos] == '-')
        sign = -1, pos++;

    while (pos < value.len() && isdigit(value[pos]))
        i = (i * 10) + (value[pos++] - '0');

    if (pos == value.len()) // succesfully parsed as int
        return sign * i;
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
            errorExit("bad string literal!");

        return to_string(value(1, value.len()-1));
    } else if (value[value.len()-1] == '$') // string variable
        return get_var_str(to_string(value));
    else { // int variable
        char buf[32];
        sprintf(buf, "%02d", get_var_int(to_string(value)));
        return std::string(buf);
    }
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

static bool is_equal(const Slice &value, const char *str)
{
    U32 pos = 0;
    while (pos < value.len() && str[pos] && tolower(value[pos]) == str[pos])
        pos++;
    return pos == value.len();
}

// ---- command functions

static void cmd_if()
{
    printf("if: ");
    print(line);
    printf("\n");
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

static void cmd_pic()
{
    Slice filename = scan_word();
    load_background(to_string(filename).c_str());

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
    // TODO use this :)
    flag = flag;
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
        errorExit("unknown fade direction");
}

static void cmd_exec()
{
    Slice what = scan_word();
    if (is_equal(what, "dialog")) {
        Slice charname = scan_word();
        Slice dlgname = scan_word();
        // TODO run dialog!
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

static struct CommandDesc
{
    char *name;
    int prefixlen;
    void (*exec)();
} commands[] = {
    "if",           2,  cmd_if,
    "set",          2,  cmd_set,
    "add",          2,  cmd_add,
    "pic",          2,  cmd_pic,
    "keyenable",    2,  cmd_keyenable,
    "song",         2,  cmd_song,
    "fade",         2,  cmd_fade,
    "exec",         2,  cmd_exec,
    "random",       2,  cmd_random,
};

void run_script(Slice code, bool init)
{
    // init scan
    scan = code;
    isInit = init;

    // scan script
    while (scan.len()) {
        scan_line();
        skip_whitespace();

        Slice command = scan_word();
        int i;
        for (i=0; i < ARRAY_COUNT(commands); i++) {
            int j=0;
            while (j < commands[i].prefixlen && tolower(command[j]) == commands[i].name[j])
                j++;
            if (j == commands[i].prefixlen) {
                commands[i].exec();
                break;
            }
        }

        if (i == ARRAY_COUNT(commands)) {
            print(command);
            printf("?\n");
        }
    }
}