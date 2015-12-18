#include "pebble.h"
#include "pebble_fonts.h"

#include "PebbleDialer.h"
#include "util.h"
#include "CircularBuffer.h"

static Window* window;

static uint16_t numEntries = 10;

static int16_t pickedEntry = -1;
static uint8_t pickedMode = 0;

typedef struct
{
	char name[21];
	char date[21];
	uint8_t type;
	char number[21];
} CallLogEntry;

static CircularBuffer* callLogData;

static MenuLayer* menuLayer;

static GBitmap* incomingCall;
static GBitmap* outgoingCall;
static GBitmap* missedCall;

static StatusBarLayer* statusBar;

static uint16_t scrollPosition = 0;

static void requestNumbers(uint16_t pos)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 2);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint16(iterator, 2, pos);

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_message_outbox_send();
}

static void sendpickedEntry(int16_t pos, uint8_t mode)
{
	DictionaryIterator *iterator;
	AppMessageResult result = app_message_outbox_begin(&iterator);
	if (result != APP_MSG_OK)
	{
			pickedEntry = pos;
			pickedMode = mode;
			return;
	}

	dict_write_uint8(iterator, 0, 2);
	dict_write_uint8(iterator, 1, 1);
	dict_write_int16(iterator, 2, pos);
	dict_write_uint8(iterator, 3, mode);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_message_outbox_send();

	pickedEntry = -1;
}

static void requestAdditionalEntries(void)
{
	uint16_t filledDown = cb_getNumOfLoadedSpacesDownFromCenter(callLogData, numEntries);
	uint16_t filledUp = cb_getNumOfLoadedSpacesUpFromCenter(callLogData);


	if (filledDown < 4 && filledDown <= filledUp)
	{
		uint16_t startingIndex = callLogData->centerIndex + filledDown;
		requestNumbers(startingIndex);
	}
	else if (filledUp < 4)
	{
		uint16_t startingIndex = callLogData->centerIndex - filledUp;
		requestNumbers(startingIndex);
	}
}

static uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
	return 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	return numEntries;
}


static int16_t menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data) {
	CallLogEntry* entry = cb_getEntry(callLogData, cell_index->row);
	if (entry == NULL || entry->number[0] == 0)
		return 40; //Return smaller window if there is no number (third row) to display
	else
		return 55;
}

static void menu_pos_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
	cb_shift(callLogData, new_index.row);
	requestAdditionalEntries();
}


static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
	CallLogEntry* callLogEntry = cb_getEntry(callLogData, cell_index->row);
	if (callLogEntry == NULL)
		return;

	bool hasNumberType = callLogEntry->number[0] != 0;

	if (menu_cell_layer_is_highlighted(cell_layer))
		graphics_context_set_text_color(ctx, GColorWhite);
	else
		graphics_context_set_text_color(ctx, GColorBlack);

	graphics_draw_text(ctx, callLogEntry->name, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(35, 0, SCREEN_WIDTH - 30, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
	graphics_draw_text(ctx, callLogEntry->date, fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(35, 20, SCREEN_WIDTH - 30, 15), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
	if (hasNumberType)
		graphics_draw_text(ctx, callLogEntry->number, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(35, 35, SCREEN_WIDTH - 30, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

	GBitmap* image;
	switch (callLogEntry->type)
	{
	case 1:
		image = incomingCall;
		break;
	case 2:
		image = outgoingCall;
		break;
	default:
		image = missedCall;
		break;
	}

	graphics_context_set_compositing_mode(ctx, GCompOpSet);
	graphics_draw_bitmap_in_rect(ctx, image, GRect(3, hasNumberType ? 14 : 5, 28, 28));
}


static void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	sendpickedEntry(cell_index->row, 0);
}

static void menu_long_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	sendpickedEntry(cell_index->row, 1);
}

static void receivedEntries(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 2)->value->uint16;
	numEntries = dict_find(data, 3)->value->uint16;

	CallLogEntry* entry = cb_getEntryForFilling(callLogData, offset);
	if (entry != NULL)
	{
		entry->type = dict_find(data, 4)->value->uint8;
		strcpy(entry->name, dict_find(data, 5)->value->cstring);
		strcpy(entry->date, dict_find(data, 6)->value->cstring);
		strcpy(entry->number, dict_find(data, 7)->value->cstring);
	}

	menu_layer_reload_data(menuLayer);

	if (pickedEntry >= 0)
	{
		sendpickedEntry(pickedEntry, pickedMode);
		return;
	}

	requestAdditionalEntries();
}

void call_log_window_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 0:
		receivedEntries(data);
		break;

	}
}

void call_log_window_data_sent(void)
{
	if (pickedEntry >= 0)
	{
		sendpickedEntry(pickedEntry, pickedMode);
	}
}

static void window_appear(Window *me) {
	callLogData = cb_create(sizeof(CallLogEntry), 5);

	Layer* topLayer = window_get_root_layer(window);

	menuLayer = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH, HEIGHT_BELOW_STATUSBAR));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks){
			.get_num_sections = menu_get_num_sections_callback,
			.get_num_rows = menu_get_num_rows_callback,
			.get_cell_height = menu_get_row_height_callback,
			.draw_row = menu_draw_row_callback,
			.select_click = menu_select_callback,
			.select_long_click = menu_long_select_callback,
			.selection_changed = menu_pos_changed,


	});

#ifdef PBL_COLOR
	menu_layer_set_highlight_colors(menuLayer, GColorJaegerGreen, GColorBlack);
#endif

	menu_layer_set_click_config_onto_window(menuLayer, window);

	incomingCall = gbitmap_create_with_resource(RESOURCE_ID_INCOMING_CALL);
	outgoingCall = gbitmap_create_with_resource(RESOURCE_ID_OUTGOING_CALL);
	missedCall = gbitmap_create_with_resource(RESOURCE_ID_MISSED_CALL);

	layer_add_child(topLayer, (Layer*) menuLayer);

	statusBar = status_bar_layer_create();
	layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));

	menu_layer_set_selected_index(menuLayer, MenuIndex(0, scrollPosition), MenuRowAlignCenter, false);
	requestAdditionalEntries();

	setCurWindow(2);
}

static void window_disappear(Window* me)
{
	layer_remove_child_layers(window_get_root_layer(me));

	scrollPosition = menu_layer_get_selected_index(menuLayer).row;

	gbitmap_destroy(incomingCall);
	gbitmap_destroy(outgoingCall);
	gbitmap_destroy(missedCall);

	menu_layer_destroy(menuLayer);
	status_bar_layer_destroy(statusBar);

	cb_destroy(callLogData);
}

static void window_load(Window *me) {
	scrollPosition = 0;
}

static void window_unload(Window *me) {
	window_destroy(me);
}

void call_log_window_init(void)
{
	window = window_create();

	window_set_window_handlers(window, (WindowHandlers){
		.appear = window_appear,
		.load = window_load,
		.unload = window_unload,
		.disappear = window_disappear

	});

	window_stack_push(window, true /* Animated */);
}

