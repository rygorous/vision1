#include "common.h"
#include "vars.h"
#include "util.h"
#include "str.h"
#include <ctype.h>
#include <unordered_map>

namespace std {
    template<>
    class hash<Str> {
    public:
        size_t operator()(const Str &s) const
        {
            // FNV-1a hash
            size_t hash = 2166136261;
            for (int i=0; i < s.size(); i++)
                hash = (hash ^ s[i]) * 16777619;
            return hash;
        }
    };
}

static std::unordered_map<Str, int> int_vars;
static std::unordered_map<Str, Str> str_vars;

void vars_init()
{
    // init to default values!
    set_var_str("vorname$", "");
    set_var_str("name$", "");

    // dialog stuff
    set_var_int("tom1", 0);
    set_var_int("tom2", 0);
    set_var_int("tom3", 0);
    
    // bunch of dummy stuff to test aufzug.par
    set_var_int("stufe", 0);
    set_var_int("ok", 0);
    set_var_int("money", 500);
    set_var_int("kalorie", 500);
    set_var_int("maxplo", 0);
    set_var_int("sympaok", 0);
    set_var_int("sympa", 0);
    set_var_int("kaution", 100);
    set_var_int("robot", 0);
    set_var_int("st", 0);
    set_var_int("morgen", 0);
    set_var_int("puzzle", 0);
    set_var_int("hot", 999);
    set_var_str("key$", "");
    set_var_str("multi$", "");

    set_var_int("fred", 0);
    set_var_int("axeltreff", 0);

    // debug
    set_var_int("etage", 34);
    set_var_int("gangx", 20);
    set_var_int("gangy", 1);
    set_var_int("gangd", 0);
}

void dump_all_vars()
{
    printf("ALL VARS:\n");
    for (auto it = int_vars.begin(); it != int_vars.end(); ++it)
        printf("  %s = %d\n", it->first.c_str(), it->second);
    for (auto it = str_vars.begin(); it != str_vars.end(); ++it)
        printf("  %s = %s\n", it->first.c_str(), it->second.c_str());
}

int get_var_int(const Str &name)
{
    auto iter = int_vars.find(tolower(name));
    if (iter == int_vars.end())
        panic("variable not found: %s", name);
    return iter->second;
}

void set_var_int(const Str &name, int value)
{
    int_vars[tolower(name)] = value;
}

int *get_var_int_ptr(const Str &name)
{
    auto iter = int_vars.find(tolower(name));
    if (iter == int_vars.end())
        panic("variable not found: %s", name);
    return &iter->second;
}

Str get_var_str(const Str &name)
{
    auto iter = str_vars.find(tolower(name));
    if (iter == str_vars.end())
        panic("variable not found: %s", name);
    return iter->second;
}

void set_var_str(const Str &name, const Str &value)
{
    str_vars[tolower(name)] = value;
}

Str get_var_as_str(const Str &name)
{
    if (name.size() && name.back() == '$') // string var
        return get_var_str(name);
    else // int var
        return Str::fmt("%02d", get_var_int(name));
}