#ifndef __SCRIPT_H__
#define __SCRIPT_H__

class Slice;

bool eval_bool_expr(const Slice &expr);
void game_command(const char *cmd);
void game_frame();
void game_script_tick();
void game_shutdown();

const unsigned char *game_get_screen_row(int y);

#endif