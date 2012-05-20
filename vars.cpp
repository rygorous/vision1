#include "common.h"
#include <ctype.h>
#include <unordered_map>

static std::unordered_map<std::string, int> int_vars;
static std::unordered_map<std::string, std::string> str_vars;

static std::string canonical(const std::string &in)
{
    std::string s = in;
    for (size_t i=0; i < s.size(); i++)
        s[i] = tolower(s[i]);

    return s;
}

void init_vars()
{
    // init to default values!
    set_var_str("vorname$", "");
    set_var_str("name$", "");
    
    // bunch of dummy stuff to test aufzug.par
    set_var_int("stufe", 0);
    set_var_int("ok", 0);
    set_var_int("tom3", 0);
    set_var_int("money", 500);
    set_var_int("kalorie", 500);
    set_var_int("maxplo", 0);
    set_var_int("sympaok", 0);
    set_var_int("sympa", 0);
    set_var_int("kaution", 100);
    set_var_int("robot", 0);
    set_var_int("st", 1);
    set_var_int("morgen", 0);
    set_var_int("puzzle", 0);
    set_var_int("hot", 999);
    set_var_str("key$", "");
}

int get_var_int(const std::string &name)
{
    auto iter = int_vars.find(canonical(name));
    if (iter == int_vars.end())
        errorExit("variable not found: %s", name.c_str());
    return iter->second;
}

void set_var_int(const std::string &name, int value)
{
    int_vars[canonical(name)] = value;
}

std::string get_var_str(const std::string &name)
{
    auto iter = str_vars.find(canonical(name));
    if (iter == str_vars.end())
        errorExit("variable not found: %s", name.c_str());
    return iter->second;
}

void set_var_str(const std::string &name, const std::string &value)
{
    str_vars[canonical(name)] = value;
}