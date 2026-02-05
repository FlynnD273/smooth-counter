#include <pebble.h>

#define SETTINGS_KEY 0
#define ANIM_DUR 200
#define IMG_BUFFER 5
#define RESET_CIRCLE_OFFSET 10

typedef struct AppState {
  long value;
  long every_value;
  long offset_value;
} AppState;

static Window *s_window;
static Layer *background_layer;
static TextLayer *current_layer;
static TextLayer *next_layer;
static BitmapLayer *plus_layer;
static BitmapLayer *minus_layer;
static Layer *reset_layer;
static GBitmap *plus_bitmap;
static GBitmap *minus_bitmap;
static GBitmap *reset_bitmap;
static GRect icon_bounds;
static GRect frame;
static long delta = 0;
static AppState state;
static AnimationProgress anim_progress;
static AnimationProgress reset_progress;
static bool is_resetting = false;
static char current_text[5];
static char next_text[5];

static void default_state() {
  state.value = 1;
  state.every_value = 10;
  state.offset_value = 0;
}

static void load_state() {
  default_state();
  persist_read_data(SETTINGS_KEY, &state, sizeof(AppState));
}

static void save_state() {
  persist_write_data(SETTINGS_KEY, &state, sizeof(AppState));
}

static uint16_t get_center() { return frame.size.h / 2 - 28; }

static bool is_next_highlight() {
  return state.every_value != 0 &&
         (state.value + delta - state.offset_value) % state.every_value == 0;
}
static bool is_curr_highlight() {
  return state.every_value != 0 &&
         (state.value - state.offset_value) % state.every_value == 0;
}

static void draw_reset_button(Layer *layer, GContext *context) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_compositing_mode(context, GCompOpSet);
  graphics_draw_bitmap_in_rect(
      context, reset_bitmap,
      GRect(bounds.origin.x + (bounds.size.w - icon_bounds.size.w) / 2,
            bounds.origin.y + (bounds.size.h - icon_bounds.size.h) / 2,
            icon_bounds.size.w, icon_bounds.size.h));
  if (reset_progress > 0) {
    graphics_context_set_stroke_color(
        context, is_curr_highlight() ? GColorWhite : GColorBlack);
    graphics_context_set_stroke_width(context, 2);
    graphics_draw_arc(
        context,
        GRect(bounds.origin.x + 1, bounds.origin.y + 1, bounds.size.w - 2,
              bounds.size.h - 2),
        GOvalScaleModeFitCircle, 0,
        DEG_TO_TRIGANGLE(reset_progress * 360 / ANIMATION_NORMALIZED_MAX));
  }
}

static void draw_background_layer(Layer *layer, GContext *context) {
  bool next_highlight = is_next_highlight();
  bool curr_highlight = is_curr_highlight();
  text_layer_set_text_color(next_layer,
                            next_highlight ? GColorWhite : GColorBlack);
  text_layer_set_text_color(current_layer,
                            curr_highlight ? GColorWhite : GColorBlack);
  graphics_context_set_fill_color(context, GColorBlack);
  if (curr_highlight && next_highlight) {
    graphics_fill_rect(context, frame, 0, 0);
  } else if (curr_highlight) {
    if (delta > 0) {
      graphics_fill_rect(
          context,
          GRect(0, 0, frame.size.w,
                frame.size.h * (ANIMATION_NORMALIZED_MAX - anim_progress) /
                    ANIMATION_NORMALIZED_MAX),
          0, 0);
    } else {
      graphics_fill_rect(
          context,
          GRect(0, frame.size.h * anim_progress / ANIMATION_NORMALIZED_MAX,
                frame.size.w, frame.size.h),
          0, 0);
    }
  } else if (next_highlight) {
    if (delta > 0) {
      graphics_fill_rect(
          context,
          GRect(0,
                frame.size.h * (ANIMATION_NORMALIZED_MAX - anim_progress) /
                    ANIMATION_NORMALIZED_MAX,
                frame.size.w, frame.size.h),
          0, 0);
    } else {
      graphics_fill_rect(
          context,
          GRect(0, 0, frame.size.w,
                frame.size.h * anim_progress / ANIMATION_NORMALIZED_MAX),
          0, 0);
    }
  }
}

static void curr_update(Animation *animation,
                        const AnimationProgress progress) {
  short curr_offset = progress * (frame.size.h) / ANIMATION_NORMALIZED_MAX;
  if (delta > 0) {
    curr_offset *= -1;
  }
  layer_set_frame(text_layer_get_layer(current_layer),
                  GRect(0, get_center() + curr_offset,
                        frame.size.w - IMG_BUFFER * 2 - icon_bounds.size.w,
                        42));
}

static void next_setup(Animation *animation) {
  anim_progress = 0;
  layer_set_frame(text_layer_get_layer(next_layer),
                  GRect(0, frame.size.h, frame.size.w, 42));
  snprintf(next_text, sizeof(current_text) - 1, "%ld", state.value + delta);
  text_layer_set_text(next_layer, next_text);
}

static void next_update(Animation *animation,
                        const AnimationProgress progress) {
  anim_progress = progress;
  short next_offset = (ANIMATION_NORMALIZED_MAX - progress) * frame.size.h /
                      ANIMATION_NORMALIZED_MAX;
  if (delta < 0) {
    next_offset *= -1;
  }
  layer_set_frame(text_layer_get_layer(next_layer),
                  GRect(0, get_center() + next_offset,
                        frame.size.w - IMG_BUFFER * 2 - icon_bounds.size.w,
                        42));
}

static void anim_teardown(Animation *animation) {
  state.value += delta;
  delta = 0;
  anim_progress = 0;
  save_state();
  layer_mark_dirty(background_layer);
  snprintf(current_text, sizeof(current_text) - 1, "%ld", state.value);
  text_layer_set_text(current_layer, current_text);
  text_layer_set_text(next_layer, "");
  layer_set_frame(text_layer_get_layer(current_layer),
                  GRect(0, get_center(),
                        frame.size.w - IMG_BUFFER * 2 - icon_bounds.size.w,
                        42));
}

static const AnimationImplementation curr_anim = {
    .setup = NULL, .update = curr_update, .teardown = anim_teardown};
static const AnimationImplementation next_anim = {
    .setup = next_setup, .update = next_update, .teardown = NULL};

static Animation *curr_animation;
static Animation *next_animation;

static void update_value(long change) {
  if (!animation_is_scheduled(next_animation) &&
      !animation_is_scheduled(curr_animation)) {
    delta = change;

    next_animation = animation_create();
    animation_set_implementation(next_animation, &next_anim);
    animation_set_curve(next_animation, AnimationCurveEaseOut);
    animation_set_duration(next_animation, ANIM_DUR);
    animation_schedule(next_animation);

    curr_animation = animation_create();
    animation_set_implementation(curr_animation, &curr_anim);
    animation_set_curve(curr_animation, AnimationCurveEaseIn);
    animation_set_duration(curr_animation, ANIM_DUR);
    animation_schedule(curr_animation);
  }
}

static void reset_setup(Animation *animation) {
  layer_set_bounds(reset_layer,
                   GRect(0, 0, icon_bounds.size.w + RESET_CIRCLE_OFFSET,
                         icon_bounds.size.h + RESET_CIRCLE_OFFSET));
  reset_progress = 0;
}

static void reset_update(Animation *animation,
                         const AnimationProgress progress) {
  reset_progress = progress;
  layer_mark_dirty(reset_layer);
}

static void reset_teardown(Animation *animation) {
  reset_progress = 0;
  layer_set_bounds(reset_layer,
                   GRect(IMG_BUFFER / 2, 0,
                         icon_bounds.size.w + RESET_CIRCLE_OFFSET,
                         icon_bounds.size.h + RESET_CIRCLE_OFFSET));
  if (is_resetting) {
    is_resetting = false;
    update_value(-state.value);
  }
}

void down_click(ClickRecognizerRef recognizer, void *context) {
  update_value(1);
  layer_set_bounds(bitmap_layer_get_layer(plus_layer),
                   GRect(0, 0, icon_bounds.size.w, icon_bounds.size.h));
}

void down_unclick(ClickRecognizerRef recognizer, void *context) {
  layer_set_bounds(
      bitmap_layer_get_layer(plus_layer),
      GRect(IMG_BUFFER / 2, 0, icon_bounds.size.w, icon_bounds.size.h));
}

void up_click(ClickRecognizerRef recognizer, void *context) {
  update_value(-1);
  layer_set_bounds(bitmap_layer_get_layer(minus_layer),
                   GRect(0, 0, icon_bounds.size.w, icon_bounds.size.h));
}

void up_unclick(ClickRecognizerRef recognizer, void *context) {
  layer_set_bounds(
      bitmap_layer_get_layer(minus_layer),
      GRect(IMG_BUFFER / 2, 0, icon_bounds.size.w, icon_bounds.size.h));
}

static Animation *reset_animation;
static const AnimationImplementation reset_anim = {
    .setup = reset_setup, .update = reset_update, .teardown = reset_teardown};

void middle_click(ClickRecognizerRef recognizer, void *context) {
  is_resetting = true;
  reset_animation = animation_create();
  animation_set_implementation(reset_animation, &reset_anim);
  animation_set_curve(reset_animation, AnimationCurveEaseInOut);
  animation_set_duration(reset_animation, 500);
  animation_schedule(reset_animation);
}

void middle_unclick(ClickRecognizerRef recognizer, void *context) {
  if (is_resetting) {
    is_resetting = false;
    if (animation_is_scheduled(reset_animation)) {
      animation_unschedule(reset_animation);
    } else {
      update_value(-state.value);
    }
  }
}

void config_provider(Window *window) {
  window_raw_click_subscribe(BUTTON_ID_DOWN, down_click, down_unclick, NULL);
  window_raw_click_subscribe(BUTTON_ID_UP, up_click, up_unclick, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, middle_click, middle_unclick,
                             NULL);
}

static void init_text_layers(Layer *layer) {
  current_layer = text_layer_create(GRect(
      0, get_center(), frame.size.w - IMG_BUFFER * 2 - icon_bounds.size.w, 42));
  layer_add_child(layer, text_layer_get_layer(current_layer));
  text_layer_set_font(current_layer,
                      fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_color(current_layer, GColorBlack);
  text_layer_set_background_color(current_layer, GColorClear);
  text_layer_set_text_alignment(current_layer, GTextAlignmentCenter);
  snprintf(current_text, sizeof(current_text) - 1, "%ld", state.value);
  text_layer_set_text(current_layer, current_text);

  next_layer = text_layer_create(frame);
  layer_add_child(layer, text_layer_get_layer(next_layer));
  text_layer_set_font(next_layer,
                      fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_color(next_layer, GColorBlack);
  text_layer_set_background_color(next_layer, GColorClear);
  text_layer_set_text_alignment(next_layer, GTextAlignmentCenter);
}

static void init_menu_layers(Layer *layer) {
#ifdef PBL_ROUND
  short x_offset = frame.size.w / 8;
  short y_offset = frame.size.w / 10;
#else
  short x_offset = 0;
  short y_offset = 0;
#endif
  plus_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PLUS);
  icon_bounds = gbitmap_get_bounds(plus_bitmap);
  plus_layer = bitmap_layer_create(
      GRect(frame.size.w - IMG_BUFFER * 1.5 - icon_bounds.size.w - x_offset,
            frame.size.h - IMG_BUFFER - icon_bounds.size.h - y_offset,
            icon_bounds.size.w + IMG_BUFFER / 2, icon_bounds.size.h));
  bitmap_layer_set_bitmap(plus_layer, plus_bitmap);
  bitmap_layer_set_compositing_mode(plus_layer, GCompOpSet);
  layer_set_bounds(
      bitmap_layer_get_layer(plus_layer),
      GRect(IMG_BUFFER / 2, 0, icon_bounds.size.w, icon_bounds.size.h));
  layer_add_child(layer, bitmap_layer_get_layer(plus_layer));

  minus_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUS);
  minus_layer = bitmap_layer_create(
      GRect(frame.size.w - IMG_BUFFER * 1.5 - icon_bounds.size.w - x_offset,
            IMG_BUFFER + y_offset, icon_bounds.size.w + IMG_BUFFER / 2,
            icon_bounds.size.h));
  bitmap_layer_set_bitmap(minus_layer, minus_bitmap);
  bitmap_layer_set_compositing_mode(minus_layer, GCompOpSet);
  layer_set_bounds(
      bitmap_layer_get_layer(minus_layer),
      GRect(IMG_BUFFER / 2, 0, icon_bounds.size.w, icon_bounds.size.h));
  layer_add_child(layer, bitmap_layer_get_layer(minus_layer));

  reset_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RESET);
  reset_layer = layer_create(
      GRect(frame.size.w - IMG_BUFFER * 1.5 - RESET_CIRCLE_OFFSET / 2 -
                icon_bounds.size.w,
            (frame.size.h - icon_bounds.size.h - RESET_CIRCLE_OFFSET) / 2,
            icon_bounds.size.w + IMG_BUFFER * 2 + RESET_CIRCLE_OFFSET * 2,
            icon_bounds.size.h + RESET_CIRCLE_OFFSET * 2));
  layer_set_bounds(reset_layer,
                   GRect(IMG_BUFFER / 2, 0,
                         icon_bounds.size.w + RESET_CIRCLE_OFFSET,
                         icon_bounds.size.h + RESET_CIRCLE_OFFSET));
  layer_set_update_proc(reset_layer, draw_reset_button);
  layer_add_child(layer, reset_layer);
}

static void main_window_load(Window *window) {
  Layer *layer = window_get_root_layer(window);
  frame = layer_get_frame(layer);
  window_set_background_color(s_window, GColorWhite);

  background_layer = layer_create(frame);
  layer_add_child(layer, background_layer);
  layer_set_update_proc(background_layer, draw_background_layer);

  init_menu_layers(layer);
  init_text_layers(layer);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(current_layer);
  text_layer_destroy(next_layer);
  bitmap_layer_destroy(plus_layer);
  bitmap_layer_destroy(minus_layer);
  layer_destroy(reset_layer);
  gbitmap_destroy(plus_bitmap);
  gbitmap_destroy(minus_bitmap);
  gbitmap_destroy(reset_bitmap);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *tuple;
  tuple = dict_find(iter, MESSAGE_KEY_every_value);
  if (tuple) {
    state.every_value = tuple->value->int32;
  }
  tuple = dict_find(iter, MESSAGE_KEY_offset_value);
  if (tuple) {
    state.offset_value = tuple->value->int32;
  }
  tuple = dict_find(iter, MESSAGE_KEY_value);
  if (tuple) {
    update_value(tuple->value->int32 - state.value);
  } else {
    DictionaryIterator *iter;
    AppMessageResult result = app_message_outbox_begin(&iter);

    if (result == APP_MSG_OK) {
      dict_write_int32(iter, MESSAGE_KEY_value, state.value);
      result = app_message_outbox_send();
    }
  }
}

static void init() {
  load_state();
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = main_window_load,
                                           .unload = main_window_unload,
                                       });
  window_stack_push(s_window, true);
  window_set_click_config_provider(s_window,
                                   (ClickConfigProvider)config_provider);
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);
  save_state();
}

static void deinit() { window_destroy(s_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
