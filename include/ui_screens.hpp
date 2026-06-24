#pragma once
#include <lvgl.h>

#include "display.hpp"

extern lv_obj_t* logo_screen;
extern lv_obj_t* splash_screen;
extern lv_obj_t* main_screen;
extern lv_obj_t* logo_progress_bar;

void build_logo_layout();
void build_splash_layout();
void update_splash_status(const char* text);
void build_main_dashboard_layout();
void build_settings_screen();
