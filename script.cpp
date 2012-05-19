#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <ctype.h>

static std::string to_string(const Slice &sl)
{
    Slice &s = (Slice &)sl;
    return std::string(&s[0], &s[0] + s.len());
}

static Slice scan, line;
static bool isInit;
static int flow_counter;

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
    // an end-of-line comment starts with ; and counts as white space
    if (pos < line.len() && line[pos] == ';')
        pos = line.len();
    line = line(pos);
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

static bool has_prefix(const Slice &value, const char *str)
{
    U32 pos = 0;
    while (pos < value.len() && str[pos] && tolower(value[pos]) == str[pos])
        pos++;
    return pos == value.len();
}

static bool is_equal(const Slice &value, const char *str)
{
    return has_prefix(value, str) && str[value.len()] == 0;
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
            errorExit("string continued past end of line");
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
            errorExit("bad string literal!");

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
            errorExit("unknown relational op (str): %s", to_string(op).c_str());
    } else { // assume integers
        int lhs = int_value(tok);
        Slice op = scan_bool_tok();
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
            errorExit("unknown relational op (int): %s", to_string(op).c_str());
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
            errorExit("unknown boolean op: %s", to_string(tok).c_str());
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
    if (flow_counter == 0) { // normal execution
        bool cond = false;
        Slice l = line;

        // special cases first
        // TODO there's more of them!
        if (has_prefix(line, "init"))
            cond = isInit;
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
    if (flow_counter == 1)
        flow_counter = 0;
}

static void cmd_end()
{
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

static void cmd_off()
{
    // turn off music?
    //assert(0);
    printf("off?\n");
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

        printf("!!! dialog %s %s\n", to_string(charname).c_str(), to_string(dlgname).c_str());
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

static void cmd_wait()
{
    // TODO: what do we wait for?
    printf("wait\n");
}

static void cmd_jump()
{
    assert(0);
}

static void cmd_definelabel()
{
    // no semantics
}

static void cmd_stop()
{
    assert(0);
}

static void cmd_time()
{
    assert(0);
}

static void cmd_load()
{
    assert(0);
}

static void cmd_big()
{
    assert(0);
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
    "big",          2,  false,  cmd_big,
    "else",         2,  true,   cmd_else,
    "end",          2,  true,   cmd_end,
    "exec",         2,  false,  cmd_exec,
    "fade",         2,  false,  cmd_fade,
    "if",           2,  true,   cmd_if,
    "jump",         1,  false,  cmd_jump,
    "keyenable",    2,  false,  cmd_keyenable,
    "load",         2,  false,  cmd_load,
    "off",          2,  false,  cmd_off,
    "pic",          2,  false,  cmd_pic,
    "set",          2,  false,  cmd_set,
    "song",         2,  false,  cmd_song,
    "stop",         2,  false,  cmd_stop,
    "time",         2,  false,  cmd_time,
    "random",       2,  false,  cmd_random,
    "wait",         2,  false,  cmd_wait,
};

void run_script(Slice code, bool init)
{
    // init scan
    scan = code;
    isInit = init;
    flow_counter = 0;

    // scan script
    while (scan.len()) {
        assert(flow_counter >= 0);

        scan_line();
        skip_whitespace();

        Slice command = scan_word();
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
            assert(0);
        }
    }
}

