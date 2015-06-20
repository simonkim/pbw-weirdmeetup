#include "simple_analog.h"

#include "pebble.h"

#ifdef GColorDarkCandyAppleRed  
#define GColorSecondHand  GColorDarkCandyAppleRed 
#else
#define GColorSecondHand  GColorWhite  
#endif


static Window *window;
static Layer *s_simple_bg_layer, *s_date_layer, *s_hands_layer;
static TextLayer *s_day_label, *s_num_label;

static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;
static char s_num_buffer[4], s_day_buffer[6];

static int config_enable_date = 1; // 0 or 1

/* Watch background image bitmap */
static GBitmap *bitmap;
static BitmapLayer *bitmap_layer = NULL;

static void bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_draw_filled(ctx, s_tick_paths[i]);
  }
}
  
static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int16_t second_hand_length = bounds.size.w / 2;

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
  GPoint second_hand = {
    .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
  };

#ifdef PBL_PLATFORM_BASALT
  graphics_context_set_stroke_width(ctx, 2);
#endif
  // second hand
  graphics_context_set_stroke_color(ctx, GColorSecondHand);
  graphics_draw_line(ctx, second_hand, center);

#ifdef PBL_PLATFORM_BASALT
  graphics_context_set_stroke_width(ctx, 1);
#endif
  // minute/hour hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);

  // dot in the middle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 1, bounds.size.h / 2 - 1, 3, 3), 0, GCornerNone);
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "%a", t);
  text_layer_set_text(s_day_label, s_day_buffer);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(window));
}

static void load_background_image(Layer *window_layer)
{
  GRect bounds = layer_get_bounds(window_layer);
  
  bitmap_layer = bitmap_layer_create(bounds);
  bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WEIRDMEETUP_WF);  
  bitmap_layer_set_bitmap(bitmap_layer, bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer) );
  
}

/* Date Layer */
// 46, 73
#define DATE_LAYER_X  0
#define DATE_LAYER_Y  148
#define DATE_LAYER_HEIGHT  20
#define DATE_DAY_WIDTH  27
#define DATE_NUM_WIDTH  18

static void init_date_layer(Layer *window_layer, GRect bounds)
{
  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);
  
  s_day_label = text_layer_create(GRect(DATE_LAYER_X, DATE_LAYER_Y, DATE_DAY_WIDTH, DATE_LAYER_HEIGHT));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorBlack);
  text_layer_set_text_color(s_day_label, GColorWhite);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));

  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

  s_num_label = text_layer_create(GRect(DATE_LAYER_X + DATE_DAY_WIDTH, DATE_LAYER_Y, DATE_NUM_WIDTH, DATE_LAYER_HEIGHT));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorBlack);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));  
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  load_background_image(window_layer);

  if ( config_enable_date ) {
    init_date_layer(window_layer, bounds);
  }
  
  s_simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_simple_bg_layer);

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_simple_bg_layer);
  
  if ( config_enable_date ) {
    layer_destroy(s_date_layer);

    text_layer_destroy(s_day_label);
    text_layer_destroy(s_num_label);
  }
  
  layer_destroy(s_hands_layer);

  bitmap_layer_destroy(bitmap_layer);
  gbitmap_destroy(bitmap);
  
}

static void init() {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true);

  s_day_buffer[0] = '\0';
  s_num_buffer[0] = '\0';

  // init hand paths
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  }

  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);  
}

static void deinit() {
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_destroy(s_tick_paths[i]);
  }

  tick_timer_service_unsubscribe();
  window_destroy(window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
