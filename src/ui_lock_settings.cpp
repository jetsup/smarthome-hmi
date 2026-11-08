#include "ui_lock_settings.hpp"

#include "assets.hpp"
#include "lockscreen.hpp"
#include "theme.hpp"
#include "ui_screens.hpp"
#include "ui_settings.hpp"

namespace {
lockscreen::Settings g;

struct After { uint16_t sec; const char* label; };
const After kAfter[] = {{15, "15 s"}, {30, "30 s"}, {60, "1 min"}, {120, "2 min"}, {300, "5 min"}, {600, "10 min"}};
const int kAfterN = sizeof(kAfter) / sizeof(kAfter[0]);

lv_obj_t* g_afterLbl = nullptr;
lv_obj_t* g_tzLbl = nullptr;
lv_obj_t* g_clockBtn[2];
lv_obj_t* g_thumb[8];
int g_thumbN = 0;

void commit() { lockscreen::set(g); }

int afterIndex() {
  for (int i = 0; i < kAfterN; i++)
    if (kAfter[i].sec == g.lockAfter) return i;
  return 2;
}

void tzText(char* out, size_t n) {
  int m = g.tzMinutes, a = m < 0 ? -m : m;
  snprintf(out, n, "UTC%c%02d:%02d", m < 0 ? '-' : '+', a / 60, a % 60);
}

lv_obj_t* makeRow(lv_obj_t* panel, const char* title, int h = 52) {
  lv_obj_t* row = lv_obj_create(panel);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, h);
  lv_obj_set_style_bg_color(row, lv_color_hex(th_panel()), LV_PART_MAIN);
  lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_all(row, 8, LV_PART_MAIN);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* l = lv_label_create(row);
  lv_label_set_text(l, title);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_color(l, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, -2);
  return row;
}

lv_obj_t* smallBtn(lv_obj_t* row, const char* txt, lv_align_t al, int x, int w = 40) {
  lv_obj_t* b = lv_button_create(row);
  lv_obj_set_size(b, w, 26);
  lv_obj_align(b, al, x, 6);
  lv_obj_set_style_bg_color(b, lv_color_hex(th_surface()), LV_PART_MAIN);
  lv_obj_set_style_radius(b, 6, LV_PART_MAIN);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_center(l);
  return b;
}

void addSwitch(lv_obj_t* panel, const char* title, bool on, void (*cb)(bool)) {
  lv_obj_t* row = makeRow(panel, title, 40);
  lv_obj_t* sw = lv_switch_create(row);
  lv_obj_set_size(sw, 40, 20);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_hex(th_accent()), (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_event_cb(
      sw,
      [](lv_event_t* e) {
        auto f = (void (*)(bool))lv_event_get_user_data(e);
        f(lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED));
      },
      LV_EVENT_VALUE_CHANGED, (void*)cb);
}

void stepAfter(int dir) {
  int i = afterIndex() + dir;
  if (i < 0) i = 0;
  if (i >= kAfterN) i = kAfterN - 1;
  g.lockAfter = kAfter[i].sec;
  if (g_afterLbl) lv_label_set_text(g_afterLbl, kAfter[i].label);
  commit();
}

void stepTz(int dir) {
  int m = g.tzMinutes + dir * 30;
  if (m < -720) m = -720;
  if (m > 840) m = 840;
  g.tzMinutes = m;
  char b[16];
  tzText(b, sizeof(b));
  if (g_tzLbl) lv_label_set_text(g_tzLbl, b);
  commit();
}

void markClock() {
  for (int i = 0; i < 2; i++) {
    bool sel = g.clockStyle == i;
    lv_obj_set_style_bg_color(g_clockBtn[i], lv_color_hex(sel ? th_accent() : th_surface()), LV_PART_MAIN);
    lv_obj_t* l = lv_obj_get_child(g_clockBtn[i], 0);
    lv_obj_set_style_text_color(l, lv_color_hex(sel ? 0xFFFFFF : th_text()), LV_PART_MAIN);
  }
}

uint8_t thumbValue(int i) {
  if (i < lock_background_count) return i;
  bool custom = lockscreen::customBackgroundExists();
  if (custom && i == lock_background_count) return lockscreen::kCustomBackground;
  return lockscreen::kNoBackground;
}

void markThumbs() {
  for (int i = 0; i < g_thumbN; i++) {
    bool sel = thumbValue(i) == g.background;
    lv_obj_set_style_border_width(g_thumb[i], sel ? 3 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_thumb[i], lv_color_hex(sel ? th_accent() : th_surface()), LV_PART_MAIN);
  }
}
}  // namespace

void build_lock_settings_screen() {
  lv_obj_t* old_screen = lv_screen_active();
  g = lockscreen::get();

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(th_page()), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back_btn = lv_button_create(scr);
  lv_obj_set_size(back_btn, 36, 28);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 6, 6);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(th_surface()), LV_PART_MAIN);
  lv_obj_set_style_radius(back_btn, 4, LV_PART_MAIN);
  lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
  lv_obj_t* back_lbl = lv_label_create(back_btn);
  lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(back_lbl, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_center(back_lbl);
  lv_obj_add_event_cb(back_btn, [](lv_event_t*) { build_settings_screen(); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Lock screen");
  lv_obj_set_style_text_color(title, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t* div = lv_obj_create(scr);
  lv_obj_set_size(div, 280, 1);
  lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 38);
  lv_obj_set_style_bg_color(div, lv_color_hex(th_surface()), LV_PART_MAIN);
  lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);

  lv_obj_t* panel = lv_obj_create(scr);
  lv_obj_set_size(panel, 296, 192);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 45);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(panel, 5, LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

  addSwitch(panel, "Lock the screen", g.enabled, [](bool on) { g.enabled = on; commit(); });

  // Lock after: [-] value [+]
  lv_obj_t* row = makeRow(panel, "Lock after");
  lv_obj_t* minus = smallBtn(row, LV_SYMBOL_MINUS, LV_ALIGN_BOTTOM_LEFT, 0);
  g_afterLbl = lv_label_create(row);
  lv_label_set_text(g_afterLbl, kAfter[afterIndex()].label);
  lv_obj_set_style_text_font(g_afterLbl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(g_afterLbl, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_align(g_afterLbl, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_t* plus = smallBtn(row, LV_SYMBOL_PLUS, LV_ALIGN_BOTTOM_RIGHT, 0);
  lv_obj_add_event_cb(minus, [](lv_event_t*) { stepAfter(-1); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(plus, [](lv_event_t*) { stepAfter(1); }, LV_EVENT_CLICKED, nullptr);

  // Clock style
  lv_obj_t* crow = makeRow(panel, "Clock");
  const char* names[2] = {"Digital", "Analog"};
  for (int i = 0; i < 2; i++) {
    g_clockBtn[i] = smallBtn(crow, names[i], i == 0 ? LV_ALIGN_BOTTOM_LEFT : LV_ALIGN_BOTTOM_RIGHT, 0, 136);
    lv_obj_add_event_cb(
        g_clockBtn[i],
        [](lv_event_t* e) {
          g.clockStyle = (uint8_t)(intptr_t)lv_event_get_user_data(e);
          markClock();
          commit();
        },
        LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }
  markClock();

  // Background pictures
  lv_obj_t* brow = makeRow(panel, "Background", 98);
  lv_obj_t* strip = lv_obj_create(brow);
  lv_obj_set_size(strip, 276, 70);
  lv_obj_align(strip, LV_ALIGN_BOTTOM_LEFT, -4, 4);
  lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(strip, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(strip, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_column(strip, 6, LV_PART_MAIN);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_scroll_dir(strip, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(strip, LV_SCROLLBAR_MODE_OFF);
  g_thumbN = 0;
  bool custom = lockscreen::customBackgroundExists();
  int total = lock_background_count + (custom ? 1 : 0) + 1;  // pictures, yours (if uploaded), plain
  for (int i = 0; i < total && g_thumbN < 8; i++) {
    lv_obj_t* t = lv_obj_create(strip);
    lv_obj_set_size(t, 84, 62);
    lv_obj_set_style_radius(t, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(t, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(t, lv_color_hex(th_page()), LV_PART_MAIN);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(t, true, LV_PART_MAIN);
    if (i < lock_background_count) {
      lv_obj_t* im = lv_image_create(t);
      lv_image_set_src(im, lock_backgrounds[i]);
      lv_image_set_pivot(im, 0, 0);
      lv_image_set_scale(im, 66);  // 320 px -> ~82 px
      lv_obj_set_pos(im, 0, 0);
      lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
    } else if (custom && i == lock_background_count) {
      lv_obj_t* im = lv_image_create(t);
      lv_image_set_src(im, "L:/lockbg.bin");
      lv_image_set_pivot(im, 0, 0);
      lv_image_set_scale(im, 66);
      lv_obj_set_pos(im, 0, 0);
      lv_obj_remove_flag(im, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_t* l = lv_label_create(t);
      lv_label_set_text(l, "Plain");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
      lv_obj_set_style_text_color(l, lv_color_hex(th_muted()), LV_PART_MAIN);
      lv_obj_center(l);
    }
    lv_obj_add_event_cb(
        t,
        [](lv_event_t* e) {
          int idx = (int)(intptr_t)lv_event_get_user_data(e);
          g.background = thumbValue(idx);
          markThumbs();
          commit();
        },
        LV_EVENT_CLICKED, (void*)(intptr_t)i);
    g_thumb[g_thumbN++] = t;
  }
  markThumbs();

  addSwitch(panel, "24-hour clock", g.hour24, [](bool on) { g.hour24 = on; commit(); });

  // Time zone
  lv_obj_t* trow = makeRow(panel, "Time zone");
  lv_obj_t* tminus = smallBtn(trow, LV_SYMBOL_MINUS, LV_ALIGN_BOTTOM_LEFT, 0);
  g_tzLbl = lv_label_create(trow);
  char tb[16];
  tzText(tb, sizeof(tb));
  lv_label_set_text(g_tzLbl, tb);
  lv_obj_set_style_text_font(g_tzLbl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(g_tzLbl, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_align(g_tzLbl, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_t* tplus = smallBtn(trow, LV_SYMBOL_PLUS, LV_ALIGN_BOTTOM_RIGHT, 0);
  lv_obj_add_event_cb(tminus, [](lv_event_t*) { stepTz(-1); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(tplus, [](lv_event_t*) { stepTz(1); }, LV_EVENT_CLICKED, nullptr);

  // Preview
  lv_obj_t* pv = lv_button_create(panel);
  lv_obj_set_size(pv, LV_PCT(100), 34);
  lv_obj_set_style_bg_color(pv, lv_color_hex(th_accent()), LV_PART_MAIN);
  lv_obj_set_style_radius(pv, 8, LV_PART_MAIN);
  lv_obj_t* pl = lv_label_create(pv);
  lv_label_set_text(pl, "Preview the lock screen");
  lv_obj_set_style_text_font(pl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(pl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_center(pl);
  lv_obj_add_event_cb(pv, [](lv_event_t*) { lockscreen::lockNow(); }, LV_EVENT_CLICKED, nullptr);

  lv_screen_load(scr);
  if (old_screen != nullptr) lv_obj_delete(old_screen);
}
