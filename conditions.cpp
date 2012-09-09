#include "conditions.h"
#include "util.h"
#include "script.h"
#include "vars.h"
#include <assert.h>

Conditions::Conditions()
{
    reset();
}

void Conditions::reset()
{
    for (int i=0; i < NUM; i++) {
        bools[i] = false;
        bool_out[i] = nullptr;
        add_out[i] = nullptr;
        bool_set[i] = false;
        add_val[i] = 0;
    }
}

void Conditions::parse_vb(const Slice &vbfile)
{
    if (!vbfile.len())
        return;
        
    //print_hex("vbfile", vbfile);
    Slice scan = vbfile;
    while (scan.len()) {
        Slice line = chop_line(scan);
        if (!line.len())
            continue;

        Slice orig_line = line;

        int mode = tolower(line[0]);
        line = line(1);
        int index = scan_int(line);
        if (index < 0 || index >= NUM) {
            error_exit("vb: index=%d out of range!", index);
            continue;
        }

        line = eat_heading_space(line);

        switch (mode) {
        case 'v': // eval expr
            bools[index] = eval_bool_expr(line);
            break;

        case 'r': // write var (TODO: NOT support)
            bool_out[index] = get_var_int_ptr(to_string(line));
            bool_set[index] = true;
            break;

            // TODO: 'a' (add var)

        default:
            error_exit("vb: don't understand line '%.*s'\n", orig_line.len(), &orig_line[0]);
            break;
        }
    }
}

bool Conditions::is_set(int which) const
{
    if (which == 0)
        return true;
    else if (which >= 1 && which < NUM)
        return bools[which];
    else
        return false;
}

void Conditions::update(int which)
{
    assert(which >= 1 && which < NUM);
    bools[which] = bool_set[which];
    if (bool_out[which])
        *bool_out[which] = bools[which];
    if (add_out[which])
        *add_out[which] += add_val[which];
}