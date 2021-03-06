#ifndef __SCRIPT_H__
#define __SCRIPT_H__

class Slice;
class PixelSlice;
class Str;

bool eval_bool_expr(const Slice &expr);
void game_defer_command(const Str &cmd);
void game_run_command(const Str &cmd);
void game_reset();
void game_frame();
void game_script_tick();
void game_script_run(const Slice &script);
void game_reload_room();
void game_shutdown();

const unsigned char *game_get_screen_row(int y);
PixelSlice &game_get_hotspots();
void game_hotspot_define(int which, char code);
void game_hotspot_define_multi(int which, char *codes);
void game_hotspot_disable(int which); // TODO do this differently

#endif