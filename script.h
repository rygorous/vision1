#ifndef __SCRIPT_H__
#define __SCRIPT_H__

class Slice;

bool eval_bool_expr(const Slice &expr);
void game_command(const char *cmd);
void game_frame();
void game_script_tick();

#endif