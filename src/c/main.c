#include <pebble.h>

static Window *s_main_window;
static Layer *s_grid_layer;

// (0,0) Date cell: 3 separate text layers
static TextLayer *s_day_name_layer;
static TextLayer *s_month_layer;
static TextLayer *s_date_num_layer;

// (1,0) Time cell: hours top half, minutes bottom half
static TextLayer *s_hours_layer;
static TextLayer *s_minutes_layer;

// (2,0) E-Tank battery indicator
static Layer *s_etank_layer;
static int s_battery_percent = 100;

// (0,1) Heart rate: drawn red heart + number
static Layer *s_hr_icon_layer;
static TextLayer *s_hr_num_layer;

// (1,1) Mega Man sprite
static BitmapLayer *s_mega_man_layer;
static GBitmap *s_mega_man_bitmap;

// (2,1) Steps: walk icon + number
static BitmapLayer *s_walk_icon_layer;
static GBitmap *s_walk_icon_bitmap;
static TextLayer *s_steps_num_layer;

// (0,2) Weather: drawn condition icon + temperature
static Layer *s_weather_icon_layer;
static TextLayer *s_weather_num_layer;
static int s_weather_code = -1;  // WMO weather code from Open-Meteo

static GFont s_font_18;
static GFont s_font_12;
static GFont s_font_10;

#define BOX_SIZE 60
#define MARGIN_X 10
#define MARGIN_Y 24

// --- E-Tank pixel map ---
static const uint8_t ETANK_MAP[16][13] = {
  {1,1,1,2,1,2,1,2,1,2,1,1,1},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,1,0},
  {1,1,1,2,1,2,1,2,1,2,1,1,1},
};
#define ES  3
#define EGW 13
#define EGH 16
#define ELY 1
#define ELH 14

static void etank_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int ox = (bounds.size.w - EGW * ES) / 2;
  int oy = (bounds.size.h - EGH * ES) / 2;

  int fill_rows = (ELH * s_battery_percent) / 100;
  int empty_rows = ELH - fill_rows;

  GColor fill_color = s_battery_percent > 50 ? GColorCyan :
                      s_battery_percent > 20 ? GColorYellow : GColorRed;

  for (int gy = 0; gy < EGH; gy++) {
    for (int gx = 0; gx < EGW; gx++) {
      uint8_t pixel = ETANK_MAP[gy][gx];
      GColor color;
      bool is_interior = (pixel == 0 && gx >= 2 && gx <= 10 &&
                          gy >= ELY && gy <= (ELY + ELH - 1));
      if (is_interior) {
        color = ((gy - ELY) >= empty_rows) ? fill_color : GColorBlack;
      } else {
        switch (pixel) {
          case 1:  color = GColorBlue; break;
          case 2:  color = (gy >= 2 && gy <= 13) ? GColorBlack : GColorCyan; break;
          default: color = GColorBlack; break;
        }
      }
      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, GRect(ox + gx*ES, oy + gy*ES, ES, ES), 0, GCornerNone);
    }
  }

  // E letter centered on tank
  int s = 2;
  int ex = ox + 6 + (27 - 9*s) / 2;
  int ey = oy + 3 + (42 - 10*s) / 2;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(ex,       ey,       9*s, 2*s), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ex,       ey + 2*s, 2*s, 6*s), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ex,       ey + 4*s, 7*s, 2*s), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ex,       ey + 8*s, 9*s, 2*s), 0, GCornerNone);
}

// --- Red heart icon ---
static void hr_icon_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int top = 5;  // extra padding so heart stays inside cell border

  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_circle(ctx, GPoint(cx - 7, top + 8), 8);
  graphics_fill_circle(ctx, GPoint(cx + 7, top + 8), 8);
  for (int i = 0; i <= 16; i++) {
    int w = 30 - i * 2;
    if (w < 1) w = 1;
    graphics_fill_rect(ctx, GRect(cx - w/2, top + 9 + i, w, 1), 0, GCornerNone);
  }
}

// --- Weather icon helpers ---
static void draw_sun(GContext *ctx, int cx, int cy, int r) {
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_circle(ctx, GPoint(cx, cy), r);
  graphics_context_set_stroke_color(ctx, GColorYellow);
  graphics_context_set_stroke_width(ctx, 2);
  int ray = r + 4;
  int tip = r + 8;
  int dirs[8][2] = {{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
  for (int i = 0; i < 8; i++) {
    int dx = dirs[i][0], dy = dirs[i][1];
    graphics_draw_line(ctx, GPoint(cx + dx*ray, cy + dy*ray),
                            GPoint(cx + dx*tip, cy + dy*tip));
  }
}

static void draw_cloud(GContext *ctx, int cx, int cy, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(cx - 8, cy + 5), 7);
  graphics_fill_circle(ctx, GPoint(cx + 4, cy + 3), 8);  // reduced radius 9→8, moved down
  graphics_fill_circle(ctx, GPoint(cx + 15, cy + 6), 6);
  graphics_fill_rect(ctx, GRect(cx - 14, cy + 4, 36, 8), 0, GCornerNone);
}

static void weather_icon_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  // Layer starts 8px inside cell top, so y=0 here is already safe.
  // Cloud cy=6: circle2 top = 6+3-8=1 (just inside). Sun cy=17, r=8: ray tip top = 17-16=1.

  if (s_weather_code < 0) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(cx, 15), 9);
    return;
  }

  if (s_weather_code == 0) {
    draw_sun(ctx, cx, 17, 8);

  } else if (s_weather_code <= 3) {
    // Small sun upper-left, cloud center — sun r=5 needs cy>=13 for ray tip >= 0
    draw_sun(ctx, cx - 8, 13, 5);
    draw_cloud(ctx, cx + 2, 13, GColorWhite);

  } else if (s_weather_code <= 48) {
    draw_cloud(ctx, cx, 6, GColorLightGray);
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_context_set_stroke_width(ctx, 2);
    for (int i = 0; i < 3; i++) {
      graphics_draw_line(ctx, GPoint(cx - 14, 20 + i * 4), GPoint(cx + 14, 20 + i * 4));
    }

  } else if ((s_weather_code >= 71 && s_weather_code <= 77) ||
             (s_weather_code >= 85 && s_weather_code <= 86)) {
    draw_cloud(ctx, cx, 6, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorCyan);
    int sx[6] = {-12, -4, 4, 12, -8, 8};
    int sy[6] = {22, 25, 22, 25, 28, 28};
    for (int i = 0; i < 6; i++) {
      graphics_fill_circle(ctx, GPoint(cx + sx[i], sy[i]), 2);
    }

  } else if (s_weather_code <= 67) {
    draw_cloud(ctx, cx, 6, GColorLightGray);
    graphics_context_set_stroke_color(ctx, GColorCyan);
    graphics_context_set_stroke_width(ctx, 2);
    for (int i = 0; i < 4; i++) {
      int x = cx - 12 + i * 8;
      graphics_draw_line(ctx, GPoint(x, 20), GPoint(x - 3, 27));
    }

  } else if (s_weather_code <= 82) {
    draw_cloud(ctx, cx, 6, GColorDarkGray);
    graphics_context_set_stroke_color(ctx, GColorCyan);
    graphics_context_set_stroke_width(ctx, 2);
    for (int i = 0; i < 4; i++) {
      int x = cx - 12 + i * 8;
      graphics_draw_line(ctx, GPoint(x, 20), GPoint(x - 4, 29));
    }

  } else {
    draw_cloud(ctx, cx, 6, GColorDarkGray);
    graphics_context_set_stroke_color(ctx, GColorYellow);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx, GPoint(cx + 2, 19), GPoint(cx - 4, 27));
    graphics_draw_line(ctx, GPoint(cx - 4, 27), GPoint(cx + 2, 27));
    graphics_draw_line(ctx, GPoint(cx + 2, 27), GPoint(cx - 5, 35));
  }
}

// --- Grid background ---
static void grid_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  for (int y = 0; y < bounds.size.h; y += 4) {
    graphics_context_set_fill_color(ctx, (y % 8 < 4) ? GColorCyan : GColorTiffanyBlue);
    graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, 4), 0, GCornerNone);
  }
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int x = MARGIN_X + col * BOX_SIZE;
      int y = MARGIN_Y + row * BOX_SIZE;
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, GRect(x, y, BOX_SIZE, BOX_SIZE), 0, GCornerNone);
      graphics_context_set_stroke_color(ctx, GColorCyan);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_rect(ctx, GRect(x + 1, y + 1, BOX_SIZE - 2, BOX_SIZE - 2));
      graphics_context_set_stroke_color(ctx, GColorTiffanyBlue);
      graphics_draw_rect(ctx, GRect(x + 2, y + 2, BOX_SIZE - 4, BOX_SIZE - 4));
    }
  }
}

// --- Data updates ---
static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_hours_buffer[4];
  strftime(s_hours_buffer, sizeof(s_hours_buffer),
           clock_is_24h_style() ? "%H" : "%I", tick_time);
  text_layer_set_text(s_hours_layer, s_hours_buffer);

  static char s_minutes_buffer[4];
  strftime(s_minutes_buffer, sizeof(s_minutes_buffer), "%M", tick_time);
  text_layer_set_text(s_minutes_layer, s_minutes_buffer);

  static char s_day_name_buffer[8];
  strftime(s_day_name_buffer, sizeof(s_day_name_buffer), "%a", tick_time);
  text_layer_set_text(s_day_name_layer, s_day_name_buffer);

  static char s_month_buffer[8];
  strftime(s_month_buffer, sizeof(s_month_buffer), "%b", tick_time);
  text_layer_set_text(s_month_layer, s_month_buffer);

  static char s_date_num_buffer[4];
  strftime(s_date_num_buffer, sizeof(s_date_num_buffer), "%d", tick_time);
  text_layer_set_text(s_date_num_layer, s_date_num_buffer);
}

static void update_steps() {
  HealthMetric metric = HealthMetricStepCount;
  time_t start = time_start_of_today();
  time_t end = time(NULL);
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, start, end);
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    HealthValue steps = health_service_sum_today(metric);
    static char s_steps_buffer[12];
    snprintf(s_steps_buffer, sizeof(s_steps_buffer), "%d", (int)steps);
    text_layer_set_text(s_steps_num_layer, s_steps_buffer);
  } else {
    text_layer_set_text(s_steps_num_layer, "0");
  }
}

static void update_heart_rate() {
  HealthServiceAccessibilityMask hr_mask = health_service_metric_accessible(
    HealthMetricHeartRateBPM, time(NULL), time(NULL));
  if (hr_mask & HealthServiceAccessibilityMaskAvailable) {
    HealthValue hr = health_service_peek_current_value(HealthMetricHeartRateBPM);
    static char s_hr_buffer[8];
    if (hr > 0) {
      snprintf(s_hr_buffer, sizeof(s_hr_buffer), "%d", (int)hr);
    } else {
      snprintf(s_hr_buffer, sizeof(s_hr_buffer), "--");
    }
    text_layer_set_text(s_hr_num_layer, s_hr_buffer);
  } else {
    text_layer_set_text(s_hr_num_layer, "N/A");
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void battery_handler(BatteryChargeState charge_state) {
  s_battery_percent = charge_state.charge_percent;
  if (s_etank_layer) layer_mark_dirty(s_etank_layer);
}

static void health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate) {
    update_steps();
  } else if (event == HealthEventHeartRateUpdate) {
    update_heart_rate();
  } else if (event == HealthEventSignificantUpdate) {
    update_steps();
    update_heart_rate();
  }
}

static void bluetooth_handler(bool connected) {
  if (!connected) vibes_double_pulse();
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  if (temp_tuple) {
    static char s_weather_buffer[8];
    snprintf(s_weather_buffer, sizeof(s_weather_buffer), "%d\xc2\xb0", (int)temp_tuple->value->int32);
    text_layer_set_text(s_weather_num_layer, s_weather_buffer);
  }

  Tuple *code_tuple = dict_find(iterator, MESSAGE_KEY_WEATHER_CODE);
  if (code_tuple) {
    s_weather_code = (int)code_tuple->value->int32;
    if (s_weather_icon_layer) layer_mark_dirty(s_weather_icon_layer);
  }
}

// Helper: centered text layer within a cell
static TextLayer* make_text_layer(int cell_x, int cell_y, int y_off, int h, GFont font, Window *w) {
  TextLayer *layer = text_layer_create(GRect(cell_x + 2, cell_y + y_off, BOX_SIZE - 4, h));
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  text_layer_set_font(layer, font);
  layer_add_child(window_get_root_layer(w), text_layer_get_layer(layer));
  return layer;
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_grid_layer = layer_create(bounds);
  layer_set_update_proc(s_grid_layer, grid_update_proc);
  layer_add_child(window_layer, s_grid_layer);

  s_font_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MEGA_MAN_18));
  s_font_12 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MEGA_MAN_12));
  s_font_10 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MEGA_MAN_10));

  s_mega_man_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MEGAMAN_IDLE);
  s_walk_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_WALK);

  // --- (0,0) Date cell: day name / month / date number ---
  // y_off shifted up 2px, height +4px to prevent ascender clipping
  int cx0 = MARGIN_X,           ry0 = MARGIN_Y;
  s_day_name_layer = make_text_layer(cx0, ry0,  3, 20, s_font_10, window);
  s_month_layer    = make_text_layer(cx0, ry0, 20, 20, s_font_10, window);
  s_date_num_layer = make_text_layer(cx0, ry0, 38, 20, s_font_10, window);

  // --- (1,0) Time cell: hours top half, minutes bottom half ---
  int cx1 = MARGIN_X + BOX_SIZE;
  s_hours_layer   = make_text_layer(cx1, ry0,  4, 28, s_font_18, window);
  s_minutes_layer = make_text_layer(cx1, ry0, 32, 28, s_font_18, window);

  // --- (2,0) E-Tank ---
  int cx2 = MARGIN_X + 2*BOX_SIZE;
  s_etank_layer = layer_create(GRect(cx2, ry0, BOX_SIZE, BOX_SIZE));
  layer_set_update_proc(s_etank_layer, etank_update_proc);
  layer_add_child(window_layer, s_etank_layer);

  // --- (0,1) Heart rate: drawn red heart + BPM ---
  int ry1 = MARGIN_Y + BOX_SIZE;
  s_hr_icon_layer = layer_create(GRect(cx0, ry1, BOX_SIZE, 36));
  layer_set_update_proc(s_hr_icon_layer, hr_icon_update_proc);
  layer_add_child(window_layer, s_hr_icon_layer);
  s_hr_num_layer = make_text_layer(cx0, ry1, 36, 22, s_font_10, window);
  text_layer_set_text(s_hr_num_layer, "--");

  // --- (1,1) Mega Man sprite ---
  s_mega_man_layer = bitmap_layer_create(GRect(cx1, ry1, BOX_SIZE, BOX_SIZE));
  bitmap_layer_set_bitmap(s_mega_man_layer, s_mega_man_bitmap);
  bitmap_layer_set_compositing_mode(s_mega_man_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_mega_man_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_mega_man_layer));

  // --- (2,1) Steps: walk icon + count ---
  int icon_w = 25, icon_h = 23;
  int icon_ix = cx2 + (BOX_SIZE - icon_w) / 2;
  s_walk_icon_layer = bitmap_layer_create(GRect(icon_ix, ry1 + 6, icon_w, icon_h));
  bitmap_layer_set_bitmap(s_walk_icon_layer, s_walk_icon_bitmap);
  bitmap_layer_set_compositing_mode(s_walk_icon_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_walk_icon_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_walk_icon_layer));
  s_steps_num_layer = make_text_layer(cx2, ry1, 34, 24, s_font_10, window);
  text_layer_set_text(s_steps_num_layer, "0");

  // --- (0,2) Weather: drawn condition icon + temperature ---
  int ry2 = MARGIN_Y + 2*BOX_SIZE;
  s_weather_icon_layer = layer_create(GRect(cx0, ry2 + 8, BOX_SIZE, 30));
  layer_set_update_proc(s_weather_icon_layer, weather_icon_update_proc);
  layer_add_child(window_layer, s_weather_icon_layer);
  s_weather_num_layer = make_text_layer(cx0, ry2, 40, 22, s_font_10, window);
  text_layer_set_text(s_weather_num_layer, "--\xc2\xb0");
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_day_name_layer);
  text_layer_destroy(s_month_layer);
  text_layer_destroy(s_date_num_layer);
  text_layer_destroy(s_hours_layer);
  text_layer_destroy(s_minutes_layer);
  text_layer_destroy(s_hr_num_layer);
  text_layer_destroy(s_steps_num_layer);
  text_layer_destroy(s_weather_num_layer);

  layer_destroy(s_etank_layer);
  layer_destroy(s_hr_icon_layer);
  layer_destroy(s_weather_icon_layer);

  bitmap_layer_destroy(s_mega_man_layer);
  gbitmap_destroy(s_mega_man_bitmap);

  bitmap_layer_destroy(s_walk_icon_layer);
  gbitmap_destroy(s_walk_icon_bitmap);

  fonts_unload_custom_font(s_font_18);
  fonts_unload_custom_font(s_font_12);
  fonts_unload_custom_font(s_font_10);
  layer_destroy(s_grid_layer);
}

static void init() {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  health_service_events_subscribe(health_handler, NULL);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_handler
  });

  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(128, 128);

  update_time();
  battery_handler(battery_state_service_peek());
  update_steps();
  update_heart_rate();
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
