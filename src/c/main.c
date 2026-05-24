#include <pebble.h>

static Window *s_main_window;
static Layer *s_grid_layer;

static TextLayer *s_time_layer;
static TextLayer *s_day_layer;
static TextLayer *s_battery_layer;
static TextLayer *s_hr_num_layer;
static TextLayer *s_date_layer;
static TextLayer *s_steps_num_layer;
static TextLayer *s_weather_num_layer;

static BitmapLayer *s_mega_man_layer;
static GBitmap *s_mega_man_bitmap;

static BitmapLayer *s_hr_icon_layer;
static GBitmap *s_hr_icon_bitmap;

static BitmapLayer *s_walk_icon_layer;
static GBitmap *s_walk_icon_bitmap;

// E-Tank drawn in C as battery indicator
static Layer *s_etank_layer;
static int s_battery_percent = 100;

// Weather icon drawn in C
static Layer *s_weather_icon_layer;

static GFont s_font_18;
static GFont s_font_12;
static GFont s_font_10;

#define BOX_SIZE 60
#define MARGIN_X 10
#define MARGIN_Y 24

// Pixel-accurate E-Tank: 13w x 16h game pixels
// 0=black, 1=blue (GColorBlue), 2=gray (top/bottom caps get cyan)
static const uint8_t ETANK_MAP[16][13] = {
  {1,1,1,2,1,2,1,2,1,2,1,1,1},  // row  0: top cap
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  1
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  2
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  3
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  4
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  5
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  6
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  7
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  8
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row  9
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row 10
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row 11
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row 12
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row 13
  {0,1,0,0,0,0,0,0,0,0,0,1,0},  // row 14
  {1,1,1,2,1,2,1,2,1,2,1,1,1},  // row 15: bottom cap
};

#define ES 3   // scale: each game pixel = 3x3 screen pixels
#define EGW 13
#define EGH 16

#define ELY 1
#define ELH 14  // rows 1-14 (full body)

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
        int interior_row = gy - ELY;
        color = (interior_row >= empty_rows) ? fill_color : GColorBlack;
      } else {
        switch (pixel) {
          case 1:  color = GColorBlue;  break;
          case 2:  color = (gy >= 2 && gy <= 13) ? GColorBlack : GColorCyan; break;
          default: color = GColorBlack; break;
        }
      }

      graphics_context_set_fill_color(ctx, color);
      graphics_fill_rect(ctx, GRect(ox + gx*ES, oy + gy*ES, ES, ES), 0, GCornerNone);
    }
  }

  // Black pixel-art E letter centered on tank
  int s = 2;
  int ex = ox + 6 + (27 - 9*s) / 2;
  int ey = oy + 3 + (42 - 10*s) / 2;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(ex, ey,       9*s, 2*s), 0, GCornerNone); // top bar
  graphics_fill_rect(ctx, GRect(ex, ey+2*s,   2*s, 6*s), 0, GCornerNone); // left bar
  graphics_fill_rect(ctx, GRect(ex, ey+4*s,   7*s, 2*s), 0, GCornerNone); // middle bar
  graphics_fill_rect(ctx, GRect(ex, ey+8*s,   9*s, 2*s), 0, GCornerNone); // bottom bar
}

static void weather_icon_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2 - 2;
  int r = 10;

  // Simple sun: yellow circle with rays
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_fill_circle(ctx, GPoint(cx, cy), r);

  graphics_context_set_stroke_color(ctx, GColorYellow);
  graphics_context_set_stroke_width(ctx, 2);

  // 8 rays
  int ray_len = 5;
  int dirs[8][2] = {{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
  for (int i = 0; i < 8; i++) {
    int sx = cx + dirs[i][0] * (r + 2);
    int sy = cy + dirs[i][1] * (r + 2);
    int ex2 = cx + dirs[i][0] * (r + 2 + ray_len);
    int ey2 = cy + dirs[i][1] * (r + 2 + ray_len);
    graphics_draw_line(ctx, GPoint(sx, sy), GPoint(ex2, ey2));
  }
}

static void grid_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Horizontal stripe background — alternating cyan like stage select screen
  for (int y = 0; y < bounds.size.h; y += 4) {
    graphics_context_set_fill_color(ctx, (y % 8 < 4) ? GColorCyan : GColorTiffanyBlue);
    graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, 4), 0, GCornerNone);
  }

  // Cell black fills and stage-select style borders
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

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_time_buffer[8];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);

  static char s_day_buffer[8];
  strftime(s_day_buffer, sizeof(s_day_buffer), "%a", tick_time);
  text_layer_set_text(s_day_layer, s_day_buffer);

  static char s_date_buffer[16];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%b %d", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
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
    if (hr > 0) {
      static char s_hr_buffer[8];
      snprintf(s_hr_buffer, sizeof(s_hr_buffer), "%d", (int)hr);
      text_layer_set_text(s_hr_num_layer, s_hr_buffer);
    } else {
      text_layer_set_text(s_hr_num_layer, "--");
    }
  } else {
    text_layer_set_text(s_hr_num_layer, "N/A");
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void battery_handler(BatteryChargeState charge_state) {
  static char s_battery_buffer[8];
  snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", charge_state.charge_percent);
  text_layer_set_text(s_battery_layer, s_battery_buffer);

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
}

// Create a text layer centered in a cell, text in the lower portion
static TextLayer* create_cell_text_layer(int col, int row, int y_offset, int height, GFont font, Window *window) {
  TextLayer *layer = text_layer_create(
    GRect(MARGIN_X + (col * BOX_SIZE) + 2, MARGIN_Y + (row * BOX_SIZE) + y_offset, BOX_SIZE - 4, height));
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  text_layer_set_font(layer, font);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
  return layer;
}

// Create a bitmap icon centered in the top portion of a cell
static BitmapLayer* create_cell_icon_layer(int col, int row, int icon_w, int icon_h, Window *window) {
  int cx = MARGIN_X + col * BOX_SIZE + BOX_SIZE / 2 - icon_w / 2;
  int cy = MARGIN_Y + row * BOX_SIZE + 6;
  BitmapLayer *layer = bitmap_layer_create(GRect(cx, cy, icon_w, icon_h));
  bitmap_layer_set_compositing_mode(layer, GCompOpSet);
  bitmap_layer_set_alignment(layer, GAlignCenter);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer));
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
  s_hr_icon_bitmap   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_HR);
  s_walk_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICON_WALK);

  // Row 0: time, day, battery — full cell height text, centered vertically
  s_time_layer    = create_cell_text_layer(0, 0, 20, 26, s_font_12, window);
  s_day_layer     = create_cell_text_layer(1, 0, 20, 26, s_font_12, window);
  s_battery_layer = create_cell_text_layer(2, 0, 20, 26, s_font_10, window);

  // Row 1 col 0: E-Tank battery indicator
  s_etank_layer = layer_create(GRect(MARGIN_X, MARGIN_Y + BOX_SIZE, BOX_SIZE, BOX_SIZE));
  layer_set_update_proc(s_etank_layer, etank_update_proc);
  layer_add_child(window_layer, s_etank_layer);

  // Row 1 col 1: Mega Man sprite
  s_mega_man_layer = bitmap_layer_create(GRect(MARGIN_X + BOX_SIZE, MARGIN_Y + BOX_SIZE, BOX_SIZE, BOX_SIZE));
  bitmap_layer_set_bitmap(s_mega_man_layer, s_mega_man_bitmap);
  bitmap_layer_set_compositing_mode(s_mega_man_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_mega_man_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_mega_man_layer));

  // Row 1 col 2: heart rate — icon + number
  s_hr_icon_layer = create_cell_icon_layer(2, 1, 25, 26, window);
  bitmap_layer_set_bitmap(s_hr_icon_layer, s_hr_icon_bitmap);
  s_hr_num_layer = create_cell_text_layer(2, 1, 36, 20, s_font_10, window);
  text_layer_set_text(s_hr_num_layer, "--");

  // Row 2 col 0: date
  s_date_layer = create_cell_text_layer(0, 2, 20, 26, s_font_10, window);

  // Row 2 col 1: steps — walk icon + number
  s_walk_icon_layer = create_cell_icon_layer(1, 2, 25, 23, window);
  bitmap_layer_set_bitmap(s_walk_icon_layer, s_walk_icon_bitmap);
  s_steps_num_layer = create_cell_text_layer(1, 2, 36, 20, s_font_10, window);
  text_layer_set_text(s_steps_num_layer, "0");

  // Row 2 col 2: weather — drawn sun icon + temperature
  s_weather_icon_layer = layer_create(GRect(MARGIN_X + 2*BOX_SIZE, MARGIN_Y + 2*BOX_SIZE, BOX_SIZE, BOX_SIZE));
  layer_set_update_proc(s_weather_icon_layer, weather_icon_update_proc);
  layer_add_child(window_layer, s_weather_icon_layer);
  s_weather_num_layer = create_cell_text_layer(2, 2, 36, 20, s_font_10, window);
  text_layer_set_text(s_weather_num_layer, "--\xc2\xb0");
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_day_layer);
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_hr_num_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_steps_num_layer);
  text_layer_destroy(s_weather_num_layer);

  layer_destroy(s_etank_layer);
  layer_destroy(s_weather_icon_layer);

  bitmap_layer_destroy(s_mega_man_layer);
  gbitmap_destroy(s_mega_man_bitmap);

  bitmap_layer_destroy(s_hr_icon_layer);
  gbitmap_destroy(s_hr_icon_bitmap);

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
