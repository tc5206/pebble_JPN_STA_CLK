#include <pebble.h>

static Window *s_main_window;
static Layer *s_dial_layer, *s_hands_layer, *s_info_layer;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

static GBitmap *s_digit_l[10], *s_digit_s[10], *s_day_imgs[7];
static GBitmap *s_am_img, *s_pm_img, *s_disc_img, *s_colon_img;

static GPath *s_minute_hand_path, *s_hour_hand_path;
static int32_t s_anim_minute_angle, s_target_minute_angle;
static BatteryChargeState s_batt_state;
static bool s_is_connected;

static bool s_is_animating = false;

typedef struct {
  GColor BackgroundColor;
  bool ShowDigitalTime;
  bool ShowDigitalDate;
  bool ShowDigitalDay;
  uint8_t AnimDuration;
} __attribute__((__packed__)) ClaySettings;

static ClaySettings settings;

#if defined(PBL_PLATFORM_GABBRO)
  #define DIAL_RECT       GRect(0, 0, 260, 260)
  #define DIAL_RADIUS_IN  100
  #define DIAL_RADIUS_OUT 120
  #define DIGIT_Y          202
  #define DIGIT_X          (int[]){98, 114, 133, 149}
  #define INFO_Y           119
  #define DAY_X            37
  #define DATE_X           (int[]){191, 207}
  #define AM_RECT          GRect(143, 223, 21, 11)
  #define PM_RECT          GRect(143, 223, 21, 11)
  #define DISC_RECT        GRect(129, 256, 4, 3)
  #define SCREEN_W         260
#elif defined(PBL_PLATFORM_CHALK)
  #define DIAL_RECT       GRect(0, 0, 180, 180)
  #define DIAL_RADIUS_IN  70
  #define DIAL_RADIUS_OUT 82
  #define DIGIT_Y          138
  #define DIGIT_X          (int[]){64, 76, 93, 105}
  #define INFO_Y           82
  #define DAY_X            32
  #define DATE_X           (int[]){124, 136}
  #define AM_RECT          GRect(101, 153, 15, 8)
  #define PM_RECT          GRect(101, 153, 15, 8)
  #define DISC_RECT        GRect(89, 176, 4, 3)
  #define SCREEN_W         180
#elif defined(PBL_PLATFORM_EMERY)
  #define DIAL_RECT       GRect(0, 7, 200, 157)
  #define DIAL_RADIUS_IN  58
  #define DIAL_RADIUS_OUT 72
  #define DIGIT_Y          177
  #define DIGIT_X          (int[]){52, 75, 104, 127}
  #define INFO_Y           185
  #define DAY_X            5
  #define DATE_X           (int[]){162, 179}
  #define AM_RECT          GRect(104, 214, 21, 11)
  #define PM_RECT          GRect(127, 214, 21, 11)
  #define DISC_RECT        GRect(98, 223, 4, 3)
  #define BATT_Y           227
  #define SCREEN_W         200
#else
  #define DIAL_RECT       GRect(0, 0, 144, 120)
  #define DIAL_RADIUS_IN  42
  #define DIAL_RADIUS_OUT 52
  #define DIGIT_Y          131
  #define DIGIT_X          (int[]){37, 54, 75, 92}
  #define INFO_Y           137
  #define DAY_X            3
  #define DATE_X           (int[]){117, 130}
  #define AM_RECT          GRect(74, 157, 15, 8)
  #define PM_RECT          GRect(90, 157, 15, 8)
  #define DISC_RECT        GRect(70, 163, 4, 3)
  #define BATT_Y           167
  #define SCREEN_W         144
#endif

#if defined(PBL_PLATFORM_GABBRO)
static const GPathInfo MINUTE_HAND_POINTS = { 4, (GPoint []){{-3,-85},{3,-85},{5,22},{-5,22}} };
static const GPathInfo HOUR_HAND_POINTS   = { 4, (GPoint []){{-4,-55},{4,-55},{6,22},{-6,22}} };
#elif defined(PBL_PLATFORM_CHALK)
static const GPathInfo MINUTE_HAND_POINTS = { 4, (GPoint []){{-2,-60},{2,-60},{4,18},{-4,18}} };
static const GPathInfo HOUR_HAND_POINTS   = { 4, (GPoint []){{-3,-38},{3,-38},{5,18},{-5,18}} };
#elif defined(PBL_PLATFORM_EMERY)
static const GPathInfo MINUTE_HAND_POINTS = { 4, (GPoint []){{-2,-66},{2,-66},{4,20},{-4,20}} };
static const GPathInfo HOUR_HAND_POINTS   = { 4, (GPoint []){{-3,-42},{3,-42},{5,20},{-5,20}} };
#else
static const GPathInfo MINUTE_HAND_POINTS = { 4, (GPoint []){{-2,-48},{2,-48},{3,15},{-3,15}} };
static const GPathInfo HOUR_HAND_POINTS   = { 4, (GPoint []){{-3,-30},{3,-30},{4,15},{-4,15}} };
#endif

static void prv_default_settings() {
  settings.BackgroundColor = GColorWhite;
  settings.ShowDigitalTime = true;
  settings.ShowDigitalDate = true;
  settings.ShowDigitalDay = true;
  settings.AnimDuration = 1;
}

static void prv_save_settings() {
  persist_write_data(0, &settings, sizeof(settings));
  if (s_main_window) {
    window_set_background_color(s_main_window, settings.BackgroundColor);
  }
  layer_mark_dirty(s_info_layer);
  layer_mark_dirty(s_dial_layer);
  layer_mark_dirty(s_hands_layer);
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  if (bg_color_t) settings.BackgroundColor = GColorFromHEX(bg_color_t->value->int32);
  Tuple *show_time_t = dict_find(iter, MESSAGE_KEY_ShowDigitalTime);
  if (show_time_t) settings.ShowDigitalTime = show_time_t->value->int8;
  Tuple *show_date_t = dict_find(iter, MESSAGE_KEY_ShowDigitalDate);
  if (show_date_t) settings.ShowDigitalDate = show_date_t->value->int8;
  Tuple *show_day_t = dict_find(iter, MESSAGE_KEY_ShowDigitalDay);
  if (show_day_t) settings.ShowDigitalDay = show_day_t->value->int8;
  Tuple *anim_t = dict_find(iter, MESSAGE_KEY_AnimDuration);
  if (anim_t) settings.AnimDuration = (uint8_t)anim_t->value->int8;
  prv_save_settings();
}

static void dial_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer); 
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, (SCREEN_W >= 180) ? 3 : 2);
  for (int i = 0; i < 12; i++) {
    if (i % 3 == 0) continue; 
    int in_r = DIAL_RADIUS_IN, out_r = DIAL_RADIUS_OUT;
#if !defined(PBL_ROUND)
    if (i == 2 || i == 4 || i == 8 || i == 10) { in_r += 8; out_r += 8; }
    else { in_r += 2; out_r += 2; }
#endif
    int32_t angle = TRIG_MAX_ANGLE * i / 12;
    GPoint p_in = GPoint(center.x + (in_r * (int32_t)sin_lookup(angle) / TRIG_MAX_RATIO),
                         center.y - (in_r * (int32_t)cos_lookup(angle) / TRIG_MAX_RATIO));
    GPoint p_out = GPoint(center.x + (out_r * (int32_t)sin_lookup(angle) / TRIG_MAX_RATIO),
                          center.y - (out_r * (int32_t)cos_lookup(angle) / TRIG_MAX_RATIO));
    graphics_draw_line(ctx, p_in, p_out);
  }
}

static void draw_digital_info(GContext *ctx, struct tm *t) {
  int hr = t->tm_hour;
  int mn = t->tm_min;
  int mday = t->tm_mday;
  int wday = t->tm_wday;

#if defined(PBL_ROUND)
  if (settings.ShowDigitalTime) {
    int dw = (SCREEN_W == 260) ? 15 : 11, dh = (SCREEN_W == 260) ? 19 : 14;
    int display_hr = hr;
    if (!clock_is_24h_style()) {
      if (display_hr < 12) graphics_draw_bitmap_in_rect(ctx, s_am_img, AM_RECT);
      else graphics_draw_bitmap_in_rect(ctx, s_pm_img, PM_RECT);
      display_hr = display_hr % 12; if (display_hr == 0) display_hr = 12;
      if (display_hr >= 10) graphics_draw_bitmap_in_rect(ctx, s_digit_s[display_hr / 10], GRect(DIGIT_X[0], DIGIT_Y, dw, dh));
    } else {
      graphics_draw_bitmap_in_rect(ctx, s_digit_s[display_hr / 10], GRect(DIGIT_X[0], DIGIT_Y, dw, dh));
    }
    graphics_draw_bitmap_in_rect(ctx, s_digit_s[display_hr % 10], GRect(DIGIT_X[1], DIGIT_Y, dw, dh));
    if (SCREEN_W == 260) graphics_draw_bitmap_in_rect(ctx, s_colon_img, GRect(130, 208, 3, 7));
    else graphics_draw_bitmap_in_rect(ctx, s_colon_img, GRect(89, 141, 3, 7));
    graphics_draw_bitmap_in_rect(ctx, s_digit_s[mn / 10], GRect(DIGIT_X[2], DIGIT_Y, dw, dh));
    graphics_draw_bitmap_in_rect(ctx, s_digit_s[mn % 10], GRect(DIGIT_X[3], DIGIT_Y, dw, dh));
  }
  if (settings.ShowDigitalDay) {
    int day_w = (SCREEN_W == 260) ? 34 : 24, day_h = (SCREEN_W == 260) ? 19 : 14;
    graphics_draw_bitmap_in_rect(ctx, s_day_imgs[wday], GRect(DAY_X, INFO_Y, day_w, day_h));
  }
  if (settings.ShowDigitalDate) {
    int date_w = (SCREEN_W == 260) ? 15 : 11, date_h = (SCREEN_W == 260) ? 19 : 14;
    graphics_draw_bitmap_in_rect(ctx, s_digit_s[mday / 10], GRect(DATE_X[0], INFO_Y, date_w, date_h));
    graphics_draw_bitmap_in_rect(ctx, s_digit_s[mday % 10], GRect(DATE_X[1], INFO_Y, date_w, date_h));
  }
#else
  if (settings.ShowDigitalTime) {
    int display_hr = hr;
    int time_w = (SCREEN_W == 200) ? 21 : 16, time_h = (SCREEN_W == 200) ? 35 : 25;
    graphics_context_set_fill_color(ctx, GColorBlack);
    
    if (clock_is_24h_style()) {
#if defined(PBL_PLATFORM_EMERY)
      graphics_fill_rect(ctx, GRect(104, 214, 44, 11), 0, GCornerNone);
#else
      graphics_fill_rect(ctx, GRect(74, 157, 31, 8), 0, GCornerNone);
#endif
      graphics_draw_bitmap_in_rect(ctx, s_digit_l[display_hr / 10], GRect(DIGIT_X[0], DIGIT_Y, time_w, time_h));
    } else {
      if (display_hr < 12) {
        graphics_draw_bitmap_in_rect(ctx, s_am_img, AM_RECT);
#if defined(PBL_PLATFORM_EMERY)
        graphics_fill_rect(ctx, GRect(127, 214, 21, 11), 0, GCornerNone);
#else
        graphics_fill_rect(ctx, GRect(90, 157, 15, 8), 0, GCornerNone);
#endif
      } else {
        graphics_draw_bitmap_in_rect(ctx, s_pm_img, PM_RECT);
#if defined(PBL_PLATFORM_EMERY)
        graphics_fill_rect(ctx, GRect(104, 214, 21, 11), 0, GCornerNone);
#else
        graphics_fill_rect(ctx, GRect(74, 157, 15, 8), 0, GCornerNone);
#endif
      }
      display_hr = display_hr % 12; if (display_hr == 0) display_hr = 12;
      
      if (display_hr >= 10) {
        graphics_draw_bitmap_in_rect(ctx, s_digit_l[display_hr / 10], GRect(DIGIT_X[0], DIGIT_Y, time_w, time_h));
      } else {
        graphics_fill_rect(ctx, GRect(DIGIT_X[0], DIGIT_Y, time_w, time_h), 0, GCornerNone);
      }
    }
    graphics_draw_bitmap_in_rect(ctx, s_digit_l[display_hr % 10], GRect(DIGIT_X[1], DIGIT_Y, time_w, time_h));
    graphics_draw_bitmap_in_rect(ctx, s_digit_l[mn / 10], GRect(DIGIT_X[2], DIGIT_Y, time_w, time_h));
    graphics_draw_bitmap_in_rect(ctx, s_digit_l[mn % 10], GRect(DIGIT_X[3], DIGIT_Y, time_w, time_h));
  }
  if (settings.ShowDigitalDay) {
    int day_w = (SCREEN_W == 200) ? 34 : 24, day_h = (SCREEN_W == 200) ? 19 : 14;
    graphics_draw_bitmap_in_rect(ctx, s_day_imgs[wday], GRect(DAY_X, INFO_Y, day_w, day_h));
  }
  if (settings.ShowDigitalDate) {
    int date_w = (SCREEN_W == 200) ? 15 : 11, date_h = (SCREEN_W == 200) ? 19 : 14;
    graphics_draw_bitmap_in_rect(ctx, s_digit_s[mday / 10], GRect(DATE_X[0], INFO_Y, date_w, date_h));
    graphics_draw_bitmap_in_rect(ctx, s_digit_s[mday % 10], GRect(DATE_X[1], INFO_Y, date_w, date_h));
  }
#endif
}

static void info_layer_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  if (s_is_animating) {
    time_t prev = now - 60;
    struct tm *prev_t = localtime(&prev);
    draw_digital_info(ctx, prev_t);
  }
  struct tm *t = localtime(&now);
  draw_digital_info(ctx, t);
#if !defined(PBL_ROUND)
  int batt_w = (SCREEN_W * s_batt_state.charge_percent) / 100, batt_x = (SCREEN_W / 2) - (batt_w / 2);
  graphics_context_set_stroke_color(ctx, settings.BackgroundColor); 
  graphics_draw_line(ctx, GPoint(batt_x, BATT_Y), GPoint(batt_x + batt_w, BATT_Y));
#endif
  if (!s_is_connected) graphics_draw_bitmap_in_rect(ctx, s_disc_img, DISC_RECT);
}

static void hands_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer); GPoint center = GPoint(b.size.w / 2, b.size.h / 2);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  time_t now = time(NULL); 
  struct tm *t = localtime(&now);
  int32_t h_angle = (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 120) + (t->tm_min * 2) + (t->tm_sec / 30))) / (12 * 120);
  gpath_rotate_to(s_hour_hand_path, h_angle); gpath_move_to(s_hour_hand_path, center);
  gpath_draw_filled(ctx, s_hour_hand_path); gpath_draw_outline(ctx, s_hour_hand_path);
  gpath_rotate_to(s_minute_hand_path, s_anim_minute_angle); gpath_move_to(s_minute_hand_path, center);
  gpath_draw_filled(ctx, s_minute_hand_path); gpath_draw_outline(ctx, s_minute_hand_path);
  graphics_context_set_fill_color(ctx, settings.BackgroundColor); 
  graphics_fill_circle(ctx, center, (SCREEN_W >= 180) ? 2 : 1);
}

static void timer_callback(void *data) { 
  s_anim_minute_angle = s_target_minute_angle; 
  s_is_animating = false;
  layer_mark_dirty(s_hands_layer); 
  layer_mark_dirty(s_info_layer);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (tick_time->tm_sec == 0 || tick_time->tm_sec == 30) {
    int32_t ang = (TRIG_MAX_ANGLE * tick_time->tm_min / 60) + (tick_time->tm_sec == 30 ? TRIG_MAX_ANGLE / 120 : 0);
    s_target_minute_angle = ang; 

    if (settings.AnimDuration > 0) {
      s_anim_minute_angle = ang + (TRIG_MAX_ANGLE / 270);
      if (tick_time->tm_sec == 0) {
        s_is_animating = true;
        layer_mark_dirty(s_info_layer);
      }
      layer_mark_dirty(s_hands_layer); 
      app_timer_register(settings.AnimDuration * 40, timer_callback, NULL);
    } else {
      s_anim_minute_angle = ang;
      s_is_animating = false;
      layer_mark_dirty(s_hands_layer);
      if (tick_time->tm_sec == 0) {
        layer_mark_dirty(s_info_layer);
      }
    }
  }
}

static void battery_handler(BatteryChargeState state) { s_batt_state = state; layer_mark_dirty(s_info_layer); }
static void connection_handler(bool connected) { s_is_connected = connected; if(!connected) vibes_short_pulse(); layer_mark_dirty(s_info_layer); }

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window); GRect b = layer_get_bounds(root);
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BACK);
  s_background_layer = bitmap_layer_create(b);
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
  layer_add_child(root, bitmap_layer_get_layer(s_background_layer));
  s_dial_layer = layer_create(DIAL_RECT); layer_set_update_proc(s_dial_layer, dial_layer_update_proc); layer_add_child(root, s_dial_layer);
  s_info_layer = layer_create(b); layer_set_update_proc(s_info_layer, info_layer_update_proc); layer_add_child(root, s_info_layer);
  s_hands_layer = layer_create(DIAL_RECT); layer_set_update_proc(s_hands_layer, hands_layer_update_proc); layer_add_child(root, s_hands_layer);
  s_hour_hand_path = gpath_create(&HOUR_HAND_POINTS); s_minute_hand_path = gpath_create(&MINUTE_HAND_POINTS);
  s_am_img = gbitmap_create_with_resource(RESOURCE_ID_AM); s_pm_img = gbitmap_create_with_resource(RESOURCE_ID_PM);
  s_disc_img = gbitmap_create_with_resource(RESOURCE_ID_DISC); s_colon_img = gbitmap_create_with_resource(RESOURCE_ID_COLON);
  
  s_digit_l[0] = gbitmap_create_with_resource(RESOURCE_ID_L0);
  s_digit_l[1] = gbitmap_create_with_resource(RESOURCE_ID_L1);
  s_digit_l[2] = gbitmap_create_with_resource(RESOURCE_ID_L2);
  s_digit_l[3] = gbitmap_create_with_resource(RESOURCE_ID_L3);
  s_digit_l[4] = gbitmap_create_with_resource(RESOURCE_ID_L4);
  s_digit_l[5] = gbitmap_create_with_resource(RESOURCE_ID_L5);
  s_digit_l[6] = gbitmap_create_with_resource(RESOURCE_ID_L6);
  s_digit_l[7] = gbitmap_create_with_resource(RESOURCE_ID_L7);
  s_digit_l[8] = gbitmap_create_with_resource(RESOURCE_ID_L8);
  s_digit_l[9] = gbitmap_create_with_resource(RESOURCE_ID_L9);

  s_digit_s[0] = gbitmap_create_with_resource(RESOURCE_ID_S0);
  s_digit_s[1] = gbitmap_create_with_resource(RESOURCE_ID_S1);
  s_digit_s[2] = gbitmap_create_with_resource(RESOURCE_ID_S2);
  s_digit_s[3] = gbitmap_create_with_resource(RESOURCE_ID_S3);
  s_digit_s[4] = gbitmap_create_with_resource(RESOURCE_ID_S4);
  s_digit_s[5] = gbitmap_create_with_resource(RESOURCE_ID_S5);
  s_digit_s[6] = gbitmap_create_with_resource(RESOURCE_ID_S6);
  s_digit_s[7] = gbitmap_create_with_resource(RESOURCE_ID_S7);
  s_digit_s[8] = gbitmap_create_with_resource(RESOURCE_ID_S8);
  s_digit_s[9] = gbitmap_create_with_resource(RESOURCE_ID_S9);

  s_day_imgs[0] = gbitmap_create_with_resource(RESOURCE_ID_D0);
  s_day_imgs[1] = gbitmap_create_with_resource(RESOURCE_ID_D1);
  s_day_imgs[2] = gbitmap_create_with_resource(RESOURCE_ID_D2);
  s_day_imgs[3] = gbitmap_create_with_resource(RESOURCE_ID_D3);
  s_day_imgs[4] = gbitmap_create_with_resource(RESOURCE_ID_D4);
  s_day_imgs[5] = gbitmap_create_with_resource(RESOURCE_ID_D5);
  s_day_imgs[6] = gbitmap_create_with_resource(RESOURCE_ID_D6);

  battery_handler(battery_state_service_peek()); s_is_connected = connection_service_peek_pebble_app_connection();
}

static void main_window_unload(Window *window) {
  if (s_background_bitmap) { gbitmap_destroy(s_background_bitmap); s_background_bitmap = NULL; }
  if (s_background_layer) { bitmap_layer_destroy(s_background_layer); s_background_layer = NULL; }
  if (s_dial_layer) { layer_destroy(s_dial_layer); s_dial_layer = NULL; }
  if (s_info_layer) { layer_destroy(s_info_layer); s_info_layer = NULL; }
  if (s_hands_layer) { layer_destroy(s_hands_layer); s_hands_layer = NULL; }
  if (s_hour_hand_path) { gpath_destroy(s_hour_hand_path); s_hour_hand_path = NULL; }
  if (s_minute_hand_path) { gpath_destroy(s_minute_hand_path); s_minute_hand_path = NULL; }
  if (s_am_img) { gbitmap_destroy(s_am_img); s_am_img = NULL; }
  if (s_pm_img) { gbitmap_destroy(s_pm_img); s_pm_img = NULL; }
  if (s_disc_img) { gbitmap_destroy(s_disc_img); s_disc_img = NULL; }
  if (s_colon_img) { gbitmap_destroy(s_colon_img); s_colon_img = NULL; }
  for(int i = 0; i < 10; i++) { 
    if (s_digit_l[i]) { gbitmap_destroy(s_digit_l[i]); s_digit_l[i] = NULL; }
    if (s_digit_s[i]) { gbitmap_destroy(s_digit_s[i]); s_digit_s[i] = NULL; }
  }
  for(int i = 0; i < 7; i++) {
    if (s_day_imgs[i]) { gbitmap_destroy(s_day_imgs[i]); s_day_imgs[i] = NULL; }
  }
}

static void init() {
  prv_default_settings(); persist_read_data(0, &settings, sizeof(settings));
  app_message_register_inbox_received(prv_inbox_received_handler); app_message_open(256, 256);
  s_main_window = window_create();
  window_set_background_color(s_main_window, settings.BackgroundColor);
  window_set_window_handlers(s_main_window, (WindowHandlers) { .load = main_window_load, .unload = main_window_unload });
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
  battery_state_service_subscribe(battery_handler);
  connection_service_subscribe((ConnectionHandlers){ .pebble_app_connection_handler = connection_handler });
  time_t n = time(NULL); struct tm *t = localtime(&n);
  s_target_minute_angle = (TRIG_MAX_ANGLE * t->tm_min / 60) + (t->tm_sec >= 30 ? TRIG_MAX_ANGLE / 120 : 0);
  s_anim_minute_angle = s_target_minute_angle;
}

static void deinit() {
  tick_timer_service_unsubscribe(); battery_state_service_unsubscribe(); connection_service_unsubscribe();
  app_message_deregister_callbacks(); window_destroy(s_main_window);
}

int main(void) { init(); app_event_loop(); deinit(); }