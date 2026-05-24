#include <pebble.h>

static Window *s_main_window;
static Layer *s_grid_layer;

static TextLayer *s_time_layer;
static TextLayer *s_day_layer;
static TextLayer *s_battery_layer;
static TextLayer *s_hr_layer;
static TextLayer *s_date_layer;
static TextLayer *s_steps_layer;
static TextLayer *s_weather_layer;

static BitmapLayer *s_mega_man_layer;
static GBitmap *s_mega_man_bitmap;

// E-Tank drawn in C as battery indicator
static Layer *s_etank_layer;
static int s_battery_percent = 100;

static GFont s_retro_font;

#define BOX_SIZE 60
#define MARGIN_X 10
#define MARGIN_Y 24

// Sprite: 16 wide x 18 tall game pixels, scale 3 = 48x54 screen pixels
// Col layout: B(1) b(2) B(1) liquid(8) B(1) b(2) B(1) = 16
// Row layout: border(1) cap(2) sep(1) body(10) sep(1) cap(2) border(1) = 18
#define ES 3  // etank scale
#define EW 16
#define EH 18
#define ELX 4   // liquid area start col
#define ELY 4   // liquid area start row
#define ELW 8   // liquid area width (game pixels)
#define ELH 10  // liquid area height (game pixels)

static void etank_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int ox = (bounds.size.w - EW * ES) / 2;
  int oy = (bounds.size.h - EH * ES) / 2;

  // Black background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(ox, oy, EW * ES, EH * ES), 0, GCornerNone);

  // TOP CAP
  // Row 1: blue cols 1-2 and 13-14, gray cols 3-12
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, GRect(ox+1*ES, oy+1*ES, 2*ES, ES), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ox+13*ES, oy+1*ES, 2*ES, ES), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(ox+3*ES, oy+1*ES, 10*ES, ES), 0, GCornerNone);

  // Row 2: blue col 1 and 14, gray cols 2-13
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, GRect(ox+1*ES, oy+2*ES, ES, ES), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ox+14*ES, oy+2*ES, ES, ES), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(ox+2*ES, oy+2*ES, 12*ES, ES), 0, GCornerNone);

  // BODY - blue side strips rows 4-13
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, GRect(ox+1*ES, oy+ELY*ES, 2*ES, ELH*ES), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ox+13*ES, oy+ELY*ES, 2*ES, ELH*ES), 0, GCornerNone);

  // LIQUID AREA fill/drain
  int lx = ox + ELX * ES;
  int ly = oy + ELY * ES;
  int lw = ELW * ES;
  int lh = ELH * ES;

  int fill_h = (lh * s_battery_percent) / 100;
  int empty_h = lh - fill_h;

  // Empty top portion (black)
  if (empty_h > 0) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(lx, ly, lw, empty_h), 0, GCornerNone);
  }

  // Filled bottom portion (color shifts with level)
  if (fill_h > 0) {
    GColor fill_color = s_battery_percent > 50 ? GColorCyan :
                        s_battery_percent > 20 ? GColorYellow : GColorRed;
    graphics_context_set_fill_color(ctx, fill_color);
    graphics_fill_rect(ctx, GRect(lx, ly + empty_h, lw, fill_h), 0, GCornerNone);
  }

  // E LETTER - always gray, drawn on top of liquid
  // 6 wide x 8 tall, offset (1,1) from liquid area top-left
  int ex = lx + 1*ES;
  int ey = ly + 1*ES;
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(ex,          ey,        6*ES, ES), 0, GCornerNone); // top bar
  graphics_fill_rect(ctx, GRect(ex,          ey+ES,     ES,   7*ES), 0, GCornerNone); // left bar
  graphics_fill_rect(ctx, GRect(ex,          ey+3*ES,   5*ES, ES), 0, GCornerNone); // middle bar
  graphics_fill_rect(ctx, GRect(ex,          ey+7*ES,   6*ES, ES), 0, GCornerNone); // bottom bar

  // BOTTOM CAP (mirror of top)
  // Row 15: blue col 1 and 14, gray cols 2-13
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, GRect(ox+1*ES,  oy+15*ES, ES,    ES), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ox+14*ES, oy+15*ES, ES,    ES), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(ox+2*ES,  oy+15*ES, 12*ES, ES), 0, GCornerNone);

  // Row 16: blue cols 1-2 and 13-14, gray cols 3-12
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, GRect(ox+1*ES,  oy+16*ES, 2*ES, ES), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(ox+13*ES, oy+16*ES, 2*ES, ES), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_rect(ctx, GRect(ox+3*ES,  oy+16*ES, 10*ES, ES), 0, GCornerNone);
}

static void grid_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlueMoon);
  graphics_context_set_stroke_width(ctx, 2);
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int x = MARGIN_X + (col * BOX_SIZE);
      int y = MARGIN_Y + (row * BOX_SIZE);
      graphics_draw_rect(ctx, GRect(x, y, BOX_SIZE, BOX_SIZE));
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
    text_layer_set_text(s_steps_layer, s_steps_buffer);
  } else {
    text_layer_set_text(s_steps_layer, "0");
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
      text_layer_set_text(s_hr_layer, s_hr_buffer);
    } else {
      text_layer_set_text(s_hr_layer, "--");
    }
  } else {
    text_layer_set_text(s_hr_layer, "N/A");
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
    text_layer_set_text(s_weather_layer, s_weather_buffer);
  }
}

static TextLayer* create_grid_text_layer(int col, int row, Window *window) {
  TextLayer *layer = text_layer_create(
    GRect(MARGIN_X + (col * BOX_SIZE), MARGIN_Y + (row * BOX_SIZE) + 15, BOX_SIZE, 30));
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  text_layer_set_font(layer, s_retro_font);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
  return layer;
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, GColorBlack);

  s_grid_layer = layer_create(bounds);
  layer_set_update_proc(s_grid_layer, grid_update_proc);
  layer_add_child(window_layer, s_grid_layer);

  s_retro_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MEGA_MAN_18));
  s_mega_man_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MEGAMAN_IDLE);

  s_time_layer    = create_grid_text_layer(0, 0, window);
  s_day_layer     = create_grid_text_layer(1, 0, window);
  s_battery_layer = create_grid_text_layer(2, 0, window);
  s_hr_layer      = create_grid_text_layer(2, 1, window);
  s_date_layer    = create_grid_text_layer(0, 2, window);
  s_steps_layer   = create_grid_text_layer(1, 2, window);
  s_weather_layer = create_grid_text_layer(2, 2, window);
  text_layer_set_text(s_weather_layer, "--\xc2\xb0");

  // E-Tank battery indicator (col 0, row 1)
  s_etank_layer = layer_create(GRect(MARGIN_X, MARGIN_Y + BOX_SIZE, BOX_SIZE, BOX_SIZE));
  layer_set_update_proc(s_etank_layer, etank_update_proc);
  layer_add_child(window_layer, s_etank_layer);

  // Mega Man sprite (col 1, row 1)
  s_mega_man_layer = bitmap_layer_create(GRect(MARGIN_X + BOX_SIZE, MARGIN_Y + BOX_SIZE, BOX_SIZE, BOX_SIZE));
  bitmap_layer_set_bitmap(s_mega_man_layer, s_mega_man_bitmap);
  bitmap_layer_set_compositing_mode(s_mega_man_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_mega_man_layer, GAlignCenter);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_mega_man_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_day_layer);
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_hr_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_steps_layer);
  text_layer_destroy(s_weather_layer);

  layer_destroy(s_etank_layer);
  bitmap_layer_destroy(s_mega_man_layer);
  gbitmap_destroy(s_mega_man_bitmap);

  fonts_unload_custom_font(s_retro_font);
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
