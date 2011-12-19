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

// ---- command functions

static void cmd_if()
{
    printf("if: ");
    print(line);
    printf("\n");
}

static void cmd_set()
{
    Slice varname = scan_word();
    if (varname[varname.len()-1] == '$')
        set_var_str(to_string(varname), str_value_word());
    else
        set_var_int(to_string(varname), int_value_word());
}

static void cmd_pic()
{
    Slice filename = scan_word();
    load_background(to_string(filename).c_str());
}

static struct CommandDesc
{
    char *name;
    int prefixlen;
    void (*exec)();
} commands[] = {
    "if",       2,  cmd_if,
    "set",      2,  cmd_set,
    "pic",      2,  cmd_pic,
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