#ifndef __VARS_H__
#define __VARS_H__

class Str;

void vars_init();

void dump_all_vars(); // debug

int get_var_int(const Str &name);
void set_var_int(const Str &name, int value);
int *get_var_int_ptr(const Str &name); // this is ugly!

Str get_var_str(const Str &name);
void set_var_str(const Str &name, const Str &value);

Str get_var_as_str(const Str &name);

#endif
