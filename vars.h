#ifndef __VARS_H__
#define __VARS_H__

#include <string>

void vars_init();

void dump_all_vars(); // debug

int get_var_int(const std::string &name);
void set_var_int(const std::string &name, int value);
int *get_var_int_ptr(const std::string &name); // this is ugly!

std::string get_var_str(const std::string &name);
void set_var_str(const std::string &name, const std::string &value);

std::string get_var_as_str(const std::string &name);

#endif
