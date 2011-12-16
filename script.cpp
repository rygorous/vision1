#include "common.h"
#include <stdio.h>
#include <ctype.h>

struct Slice
{
    U8 *start; // inclusive
    U8 *end; // exclusive

    Slice() { }
    Slice(U8 *s, U8 *e) : start(s), end(e) { }

    U8 operator [](int i) const { return (unsigned) i < (unsigned) (end - start) ? start[i] : 0; }
};

static int len(const Slice &s)
{
    return s.end - s.start;
}

static std::string to_string(const Slice &s)
{
    return std::string(s.start, s.end);
}

static Slice scan, line;
static bool isInit;

static bool islinespace(U8 ch)
{
    return ch == '\r' || ch == '\n';
}

static void scan_line()
{
    U8 *p;

    // find line_cur and line_end
    line.start = scan.start;
    for (p=scan.start; p < scan.end && !islinespace(*p); p++);
    line.end = p;

    // forward scan_cur to start of next non-empty line
    while (p < scan.end && islinespace(*p))
        p++;
    scan.start = p;
}

static void skip_whitespace()
{
    U8 *p = line.start;
    while (p < line.end && *p == ' ')
        p++;
    line.start = p;
}

static Slice scan_word()
{
    U8 *start = line.start;
    U8 *end = start;
    while (end < line.end && *end != ' ')
        end++;

    line.start = end;
    skip_whitespace();
    return Slice(start, end);
}

// ---- debug

static void print(Slice s)
{
    U8 *p = s.start;
    while (p < s.end) {
        putc(*p, stdout);
        p++;
    }
}

// ---- "higher-level" parsing

static int int_value(const Slice &value)
{
    // either a variable (which gets evaluated) or a literal
    U8 *p = value.start;
    int i = 0, sign = 1;

    // try to parse as an int
    if (p < value.end && *p == '-')
        sign = -1, p++;

    while (p < value.end && isdigit(*p))
        i = (i * 10) + (*p++ - '0');

    if (p == value.end) // succesfully parsed as int
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
        if (value[len(value)-1] != '\'')
            errorExit("bad string literal!");

        return std::string(value.start+1, value.end-1);
    } else if (value[len(value)-1] == '$') // string variable
        return get_var_str(to_string(value));
    else { // int variable
        char buf[32];
        sprintf(buf, "%02d", get_var_int(to_string(value)));
        return std::string(buf);
    }
}

static std::string str_value_word()
{
    if (line.start < line.end && *line.start == '\'') {
        // find matching '
        U8 *start = line.start;
        U8 *end = start + 1;
        while (end < line.end && *end != '\'')
            end++;
        if (end < line.end)
            end++;
        line.end = end;
        return str_value(Slice(start, end));
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
    if (varname[len(varname)-1] == '$')
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

void run_script(U8 *code, int len, bool init)
{
    // init scan
    scan = Slice(code, code + len);
    isInit = init;

    // scan script
    while (scan.start < scan.end) {
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