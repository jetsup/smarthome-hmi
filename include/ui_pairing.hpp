#pragma once
#include <lvgl.h>

void show_pairing_code_dialog();
void show_loading_dialog(const char* message);
void hide_loading_dialog();
void show_error_dialog(const char* message);
void show_pairing_result_dialog(bool success, const char* message);
