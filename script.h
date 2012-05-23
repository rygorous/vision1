#ifndef __SCRIPT_H__
#define __SCRIPT_H__

bool eval_bool_expr(const Slice &expr);
void game_frame();
void run_script(Slice code, bool init);

#endif