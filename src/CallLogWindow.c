#include "pebble.h"
#include "pebble_fonts.h"

#include "PebbleDialer.h"
#include "util.h"

static Window* window;

static uint16_t numEntries = 10;

static int8_t arrayCenterPos = 0;
static int16_t centerIndex = 0;

static int16_t pickedEntry = -1;
static uint8_t pickedMode = 0;

static char names[21][21] = {};
static char dates[21][21] = {};
static uint8_t types[21] = {};
static char numbers[21][21] = {};

static MenuLayer* menuLayer;

static GBitmap* incomingCall;
static GBitmap* outgoingCall;
static GBitmap* missedCall;

#ifdef PBL_SDK_3
	static StatusBarLayer* statusBar;
#endif

static int8_t convertToArrayPos(uint16_t index)
{
	int16_t indexDiff = index - centerIndex;
	if (indexDiff > 10 || indexDiff < -10)
		return -1;

	int8_t arrayPos = arrayCenterPos + indexDiff;
	if (arrayPos < 0)
		arrayPos += 21;
	if (arrayPos > 20)
		arrayPos -= 21;

	return arrayPos;
}

static char* getName(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return names[arrayPos];
}

static void setName(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(names[arrayPos],name);
}

static char* getDate(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return dates[arrayPos];
}

static void setDate(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(dates[arrayPos],name);
}

static uint8_t getType(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return 0;

	return types[arrayPos];
}

static void setType(uint16_t index, uint8_t type)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	types[arrayPos] = type;
}

static char* getNumber(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return numbers[arrayPos];
}

static void setNumber(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(numbers[arrayPos], name);
}

static void shiftArray(int newIndex)
{
	int8_t clearIndex;

	int16_t diff = newIndex - centerIndex;
	if (diff == 0)
		return;

	centerIndex+= diff;
	arrayCenterPos+= diff;

	if (diff > 0)
	{
		if (arrayCenterPos > 20)
			arrayCenterPos -= 21;

		clearIndex = arrayCenterPos - 10;
		if (clearIndex < 0)
			clearIndex += 21;
	}
	else
	{
		if (arrayCenterPos < 0)
			arrayCenterPos += 21;

		clearIndex = arrayCenterPos + 10;
		if (clearIndex > 20)
			clearIndex -= 21;
	}

	*names[clearIndex] = 0;
	*dates[clearIndex] = 0;
	types[clearIndex] = 0;
	*numbers[clearIndex] = 0;

}

static uint8_t getEmptySpacesDown(void)
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i <= centerIndex + 10; i++)
	{
		if (i >= numEntries)
			return 10;

		if (*getName(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

static uint8_t getEmptySpacesUp(void)
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i >= centerIndex - 10; i--)
	{
		if (i < 0)
			return 10;

		if (*getName(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

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
	int emptyDown = getEmptySpacesDown();
	int emptyUp = getEmptySpacesUp();

	if (emptyDown < 6 && emptyDown <= emptyUp)
	{
		uint8_t startingIndex = centerIndex + emptyDown;
		requestNumbers(startingIndex);
	}
	else if (emptyUp < 6)
	{
		uint8_t startingIndex = centerIndex - emptyUp;
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
	return (strlen(getNumber(cell_index->row)) == 0 ? 40 : 55);
}

static void menu_pos_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
	shiftArray(new_index.row);
	requestAdditionalEntries();
}


static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	bool hasNumberType = strlen(getNumber(cell_index->row)) > 0;

	graphics_context_set_text_color(ctx, GColorBlack);

	graphics_draw_text(ctx, getName(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(35, 0, 144 - 30, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
	graphics_draw_text(ctx, getDate(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(35, 20, 144 - 30, 15), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
	if (hasNumberType)
		graphics_draw_text(ctx, getNumber(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(35, 35, 144 - 30, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

	GBitmap* image;
	switch (getType(cell_index->row))
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

	graphics_context_set_compositing_mode(ctx, PNG_COMPOSITING_MODE);
	graphics_draw_bitmap_in_rect(ctx, image, GRect(3, hasNumberType ? 14 : 5, 28, 28));
}


static void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	sendpickedEntry(cell_index->row, 0);
}

static void menu_long_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	if (strlen(getNumber(cell_index->row)) == 0)
		return;

	sendpickedEntry(cell_index->row, 1);
}

static void receivedEntries(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 2)->value->uint16;
	numEntries = dict_find(data, 3)->value->uint16;

	setType(offset, dict_find(data, 4)->value->uint8);
	setName(offset, dict_find(data, 5)->value->cstring);
	setDate(offset, dict_find(data, 6)->value->cstring);
	setNumber(offset, dict_find(data, 7)->value->cstring);

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
	setCurWindow(2);
}

static void window_load(Window *me) {
	Layer* topLayer = window_get_root_layer(window);

	menuLayer = menu_layer_create(GRect(0, STATUSBAR_Y_OFFSET, 144, 168 - 16));

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

	layer_add_child(topLayer, (Layer*) menuLayer);

	#ifdef PBL_SDK_3
		statusBar = status_bar_layer_create();
		layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));
	#endif

}

static void window_unload(Window *me) {
	gbitmap_destroy(incomingCall);
	gbitmap_destroy(outgoingCall);
	gbitmap_destroy(missedCall);

	menu_layer_destroy(menuLayer);

	#ifdef PBL_SDK_3
		status_bar_layer_destroy(statusBar);
	#endif

	window_destroy(me);
}

void call_log_window_init(void)
{
	incomingCall = gbitmap_create_with_resource(RESOURCE_ID_INCOMING_CALL);
	outgoingCall = gbitmap_create_with_resource(RESOURCE_ID_OUTGOING_CALL);
	missedCall = gbitmap_create_with_resource(RESOURCE_ID_MISSED_CALL);

	window = window_create();

	window_set_window_handlers(window, (WindowHandlers){
		.appear = window_appear,
		.load = window_load,
		.unload = window_unload

	});

	window_stack_push(window, true /* Animated */);
}

