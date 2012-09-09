#ifndef __CONDITIONS_H__
#define __CONDITIONS_H__

class Slice;

class Conditions {
    static const int NUM = 64;

    bool bools[NUM];
    int *bool_out[NUM];
    int *add_out[NUM];

    bool bool_set[NUM];
    int add_val[NUM];
    
public:
    Conditions();

    void reset();
    void parse_vb(const Slice &slice); // .vb files contains description of conds

    bool is_set(int which) const;
    void update(int which);
};

#endif