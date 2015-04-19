#include "pebble.h"
#include "num2words-en.h"

#define DEBUG 1
#define BUFFER_SIZE 44
#define BATT_BAR_WDTH 146

static Window *window;

typedef struct {
	TextLayer *currentLayer;
	TextLayer *nextLayer;	
	PropertyAnimation *currentAnimation;
	PropertyAnimation *nextAnimation;
} Line;

TextLayer *datelayer;

static Line line1;
static Line line2;
static Line line3;
static Line line4;

static struct tm *t;
static GFont lightFont;
static GFont boldFont;
static GFont dateFont;

static char line1Str[2][BUFFER_SIZE];
static char line2Str[2][BUFFER_SIZE];
static char line3Str[2][BUFFER_SIZE];
static char line4Str[2][BUFFER_SIZE];

static bool bt_connect_toggle;
static GBitmap *bmp_bt_fg;
static BitmapLayer *layer_bt_fg;

static GBitmap *bmp_batt_bar;
static BitmapLayer *layer_batt_bar;
static Layer *layer_batt;

// Animation handler
static void animationStoppedHandler(struct Animation *animation, bool finished, void *context)
{
	Line *line = (Line*)context;
  property_animation_destroy(line->currentAnimation);
  property_animation_destroy(line->nextAnimation);
}
 
// Animate line
static void makeAnimationsForLayers(Line *line, TextLayer *current, TextLayer *next)
{
 	GRect fromRect = layer_get_frame(text_layer_get_layer(next));
 	GRect toRect = fromRect;
 	toRect.origin.x = 0;
 	fromRect.origin.x= 144;
 	
 	line->nextAnimation = property_animation_create_layer_frame(text_layer_get_layer(next), &fromRect, &toRect);
 	animation_set_duration((Animation *)line->nextAnimation, 400);
 	animation_set_curve((Animation *)line->nextAnimation, AnimationCurveEaseOut);
 	animation_schedule((Animation *)line->nextAnimation);
 	
 	GRect fromRect2 = layer_get_frame(text_layer_get_layer(current));
 	GRect toRect2 = fromRect2;
 	toRect2.origin.x = -144;
 	fromRect2.origin.x = 0;
 	
 	line->currentAnimation = property_animation_create_layer_frame(text_layer_get_layer(current), &fromRect2, &toRect2);
 	animation_set_duration((Animation *)line->currentAnimation, 400);
 	animation_set_curve((Animation *)line->currentAnimation, AnimationCurveEaseOut);
 	
 	animation_set_handlers((Animation *)line->currentAnimation, (AnimationHandlers) {
 		.stopped = (AnimationStoppedHandler)animationStoppedHandler
 	}, line);
 	
 	animation_schedule((Animation *)line->currentAnimation);
}

// Update line
static void updateLineTo(Line *line, char lineStr[2][BUFFER_SIZE], char *value)
{
	TextLayer *next, *current;
	
	GRect rect = layer_get_frame(text_layer_get_layer(line->currentLayer));
	current = (rect.origin.x == 0) ? line->currentLayer : line->nextLayer;
	next = (current == line->currentLayer) ? line->nextLayer : line->currentLayer;
	
	// Update correct text only
	if (current == line->currentLayer) {
		memset(lineStr[1], 0, BUFFER_SIZE);
		memcpy(lineStr[1], value, strlen(value));
		text_layer_set_text(next, lineStr[1]);
	} else {
		memset(lineStr[0], 0, BUFFER_SIZE);
		memcpy(lineStr[0], value, strlen(value));
		text_layer_set_text(next, lineStr[0]);
	}
	
	makeAnimationsForLayers(line, current, next);
}

// Check to see if the current line needs to be updated
static bool needToUpdateLine(Line *line, char lineStr[2][BUFFER_SIZE], char *nextValue)
{
	char *currentStr;
	GRect rect = layer_get_frame(text_layer_get_layer(line->currentLayer));
	currentStr = (rect.origin.x == 0) ? lineStr[0] : lineStr[1];

	if (memcmp(currentStr, nextValue, strlen(nextValue)) != 0 ||
		(strlen(nextValue) == 0 && strlen(currentStr) != 0)) {
		return true;
	}
	return false;
}

// Update screen based on new time
static void display_time(struct tm *t)
{
	// The current time text will be stored in the following 3 strings
	char textLine1[BUFFER_SIZE];
	char textLine2[BUFFER_SIZE];
	char textLine3[BUFFER_SIZE];
  char textLine4[BUFFER_SIZE];
  
	time_to_4words(t->tm_hour, t->tm_min, textLine1, textLine2, textLine3, textLine4, BUFFER_SIZE);
	
	if (needToUpdateLine(&line1, line1Str, textLine1)) {
		updateLineTo(&line1, line1Str, textLine1);	
	}
	if (needToUpdateLine(&line2, line2Str, textLine2)) {
		updateLineTo(&line2, line2Str, textLine2);	
	}
	if (needToUpdateLine(&line3, line3Str, textLine3)) {
		updateLineTo(&line3, line3Str, textLine3);	
	}
  if (needToUpdateLine(&line4, line4Str, textLine4)) {
		updateLineTo(&line4, line4Str, textLine4);	
	}
  
	static char dateline[20];
  //snprintf(dateline, sizeof(dateline), "%.2d-%.2d", t->tm_mday, t->tm_mon+1);
  strftime(dateline, sizeof(dateline), "%a %d %b %Y", t);
  text_layer_set_text(datelayer, dateline);
  
  layer_mark_dirty(layer_batt);
}

// Update screen without animation first time we start the watchface
static void display_initial_time(struct tm *t)
{
  time_to_4words(t->tm_hour, t->tm_min, line1Str[0], line2Str[0], line3Str[0], line4Str[0], BUFFER_SIZE);
	
	text_layer_set_text(line1.currentLayer, line1Str[0]);
	text_layer_set_text(line2.currentLayer, line2Str[0]);
	text_layer_set_text(line3.currentLayer, line3Str[0]);
  text_layer_set_text(line4.currentLayer, line4Str[0]);
  
  static char dateline[20];
  //snprintf(dateline, sizeof(dateline), "%.2d-%.2d", t->tm_mday, t->tm_mon+1);
  strftime(dateline, sizeof(dateline), "%a %d %b %Y", t);
  text_layer_set_text(datelayer, dateline);
  
  layer_mark_dirty(layer_batt);
}

static void update_batt(Layer *me, GContext* ctx)
{
  BatteryChargeState charge_state =  battery_state_service_peek();
  graphics_context_set_stroke_color(ctx,GColorWhite);
  graphics_context_set_fill_color(ctx,GColorWhite);
  graphics_fill_rect(ctx, GRect((144-BATT_BAR_WDTH)/2, 0,(BATT_BAR_WDTH*charge_state.charge_percent/100),5), 0, GCornerNone);
}

// Debug methods. For quickly debugging enable debug macro on top to transform the watchface into
// a standard app and you will be able to change the time with the up and down buttons
#if DEBUG

static void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  t->tm_min += 1;
	if (t->tm_min >= 60) {
		t->tm_min = 0;
		t->tm_hour += 1;
		
		if (t->tm_hour >= 24) {
			t->tm_hour = 0;
		}
	}
	display_time(t);
}


static void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {
  t->tm_min -= 1;
	if (t->tm_min < 0) {
		t->tm_min = 59;
		t->tm_hour -= 1;
	}
	display_time(t);
}

static void click_config_provider(ClickRecognizerRef recognizer, void *context) {
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler)up_single_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler)down_single_click_handler);
}

#endif

// Configure the first line of text
static void configureBoldLayer(TextLayer *textlayer)
{
	text_layer_set_font(textlayer, boldFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Configure for the 2nd, 3rd and 4th lines
static void configureLightLayer(TextLayer *textlayer)
{
	text_layer_set_font(textlayer, lightFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Time handler called every minute by the system
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	t = tick_time;
  display_time(tick_time);
}

static void bt_handler(bool connected) {
  if (!bt_connect_toggle && connected) {
    bt_connect_toggle = true;
    vibes_short_pulse();
    layer_set_hidden(bitmap_layer_get_layer(layer_bt_fg), true);
  }
  if (bt_connect_toggle && !connected) {
    bt_connect_toggle = false;
    vibes_long_pulse();
    layer_set_hidden(bitmap_layer_get_layer(layer_bt_fg), false);
  }
}

static void init() {
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);
  
  // Layer Overlay for BT Connection status
  bmp_bt_fg = gbitmap_create_with_resource(RESOURCE_ID_BT_FG_BLACK);
  layer_bt_fg = bitmap_layer_create(GRect(0,26,144,36));
  bitmap_layer_set_bitmap(layer_bt_fg, bmp_bt_fg);
  bitmap_layer_set_background_color(layer_bt_fg, GColorClear);
  bitmap_layer_set_compositing_mode(layer_bt_fg, GCompOpAnd);
  
  // Battery Bar
  bmp_batt_bar = gbitmap_create_with_resource(RESOURCE_ID_BATT_BAR);
  layer_batt_bar = bitmap_layer_create(GRect((144-BATT_BAR_WDTH)/2, 163, BATT_BAR_WDTH, 5));
  bitmap_layer_set_bitmap(layer_batt_bar, bmp_batt_bar);
  layer_batt = layer_create(GRect((144-BATT_BAR_WDTH)/2, 163, BATT_BAR_WDTH, 5));
  layer_set_update_proc(layer_batt, update_batt);
  
	// Custom fonts
	lightFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_GOTHAM_LIGHT_31));
	boldFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_GOTHAM_BOLD_36));
  dateFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_GOTHAM_LIGHT_14));
  
  // Date layer
  datelayer = text_layer_create(GRect(0, 0, 144, 18));
  text_layer_set_font(datelayer, dateFont);
	text_layer_set_text_color(datelayer, GColorBlack);
	text_layer_set_background_color(datelayer, GColorWhite);
	text_layer_set_text_alignment(datelayer, GTextAlignmentCenter);

	// 1st line layers
	line1.currentLayer = text_layer_create(GRect(0, 18, 144, 50));
	line1.nextLayer = text_layer_create(GRect(144, 18, 144, 50));
	configureBoldLayer(line1.currentLayer);
	configureBoldLayer(line1.nextLayer);

	// 2nd layers
	line2.currentLayer = text_layer_create(GRect(0, 55, 144, 50));
	line2.nextLayer = text_layer_create(GRect(144, 55, 144, 50));
	configureLightLayer(line2.currentLayer);
	configureLightLayer(line2.nextLayer);

	// 3rd layers
	line3.currentLayer = text_layer_create(GRect(0, 85, 144, 50));
	line3.nextLayer = text_layer_create(GRect(144, 85, 144, 50));
	configureLightLayer(line3.currentLayer);
	configureLightLayer(line3.nextLayer);
  
	// 4th layers
	line4.currentLayer = text_layer_create(GRect(0, 115, 144, 50));
	line4.nextLayer = text_layer_create(GRect(144, 115, 144, 50));
	configureLightLayer(line4.currentLayer);
	configureLightLayer(line4.nextLayer);

	// Configure time on init
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
	display_initial_time(t);

	// Load layers
	Layer *window_layer = window_get_root_layer(window);
  layer_add_child(window_layer, text_layer_get_layer(datelayer));
	layer_add_child(window_layer, text_layer_get_layer(line1.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line1.nextLayer));
	layer_add_child(window_layer, text_layer_get_layer(line2.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line2.nextLayer));
	layer_add_child(window_layer, text_layer_get_layer(line3.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line3.nextLayer));
	layer_add_child(window_layer, text_layer_get_layer(line4.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line4.nextLayer));
  layer_add_child(window_layer, bitmap_layer_get_layer(layer_bt_fg));
  layer_add_child(window_layer, bitmap_layer_get_layer(layer_batt_bar));
  layer_add_child(window_layer, layer_batt);
  layer_set_hidden(bitmap_layer_get_layer(layer_bt_fg), true);

	#if DEBUG
	// Button functionality
	window_set_click_config_provider(window, (ClickConfigProvider) click_config_provider);
	#endif

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  bluetooth_connection_service_subscribe(bt_handler);
  bt_connect_toggle = bluetooth_connection_service_peek();
  if (bt_connect_toggle) {
    layer_set_hidden(bitmap_layer_get_layer(layer_bt_fg), true);
  }
  else {
    layer_set_hidden(bitmap_layer_get_layer(layer_bt_fg), false);
  }
}

static void deinit() {
  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  bitmap_layer_destroy(layer_bt_fg);
  bitmap_layer_destroy(layer_batt_bar);
  text_layer_destroy(line1.currentLayer);
  text_layer_destroy(line1.nextLayer);
  text_layer_destroy(line2.currentLayer);
  text_layer_destroy(line2.nextLayer);
  text_layer_destroy(line3.currentLayer);
  text_layer_destroy(line3.nextLayer);
  text_layer_destroy(line4.currentLayer);
  text_layer_destroy(line4.nextLayer);
  text_layer_destroy(datelayer);
  layer_destroy(layer_batt);
  gbitmap_destroy(bmp_bt_fg);
  gbitmap_destroy(bmp_batt_bar);
  fonts_unload_custom_font(lightFont);
  fonts_unload_custom_font(boldFont);
  fonts_unload_custom_font(dateFont);
  window_destroy(window);
}

int main(void) {
  setlocale(LC_ALL, "fr_FR");
  init();
  app_event_loop();
  deinit();
}
