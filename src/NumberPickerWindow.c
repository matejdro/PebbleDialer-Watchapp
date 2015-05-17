#include "pebble.h"
#include "pebble_fonts.h"

#include "PebbleDialer.h"
#include "util.h"

static Window* window;

static uint16_t numMaxNumbers = 10;

static int8_t arrayCenterPos = 0;
static int16_t centerIndex = 0;

static int16_t pickedNumber = -1;

char numberTypes[21][16] = {};
char numbers[21][16] = {};

MenuLayer* menuLayer;

#ifdef PBL_SDK_3
	StatusBarLayer* statusBar;
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

static char* getNumberType(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return numberTypes[arrayPos];
}

static void setNumberType(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(numberTypes[arrayPos],name);
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

	strcpy(numbers[arrayPos],name);
}

static void shiftArray(int newIndex)
{
	int8_t clearIndex;

	int16_t diff = newIndex - centerIndex;
	if (diff == 0)
		return;

	centerIndex += diff;
	arrayCenterPos += diff;

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

	*numberTypes[clearIndex] = 0;
	*numbers[clearIndex] = 0;
}

static uint8_t getEmptySpacesDown(void)
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i <= centerIndex + 10; i++)
	{
		if (i >= numMaxNumbers)
			return 10;

		if (*getNumberType(i) == 0)
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

		if (*getNumberType(i) == 0)
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
	dict_write_uint8(iterator, 0, 4);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint16(iterator, 2, pos);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
}

static void sendPickedNumber(int16_t pos)
{
	DictionaryIterator *iterator;
	AppMessageResult result = app_message_outbox_begin(&iterator);
	if (result != APP_MSG_OK) {
		pickedNumber = pos;
		return;
	}

	dict_write_uint8(iterator, 0, 4);
	dict_write_uint8(iterator, 1, 1);
	dict_write_int16(iterator, 2, pos);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

	pickedNumber = -1;

}

static void requestAdditionalNumbers(void)
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
		uint8_t startingIndex = centerIndex - 1 - emptyUp;
		requestNumbers(startingIndex);
	}
}

static uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
	return 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	return numMaxNumbers;
}


static int16_t menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data) {
	return 44;
}

static void menu_pos_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
	shiftArray(new_index.row);
	requestAdditionalNumbers();
}


static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	menu_cell_basic_draw(ctx, cell_layer, getNumberType(cell_index->row), getNumber(cell_index->row), NULL);
}


static void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	sendPickedNumber(cell_index->row);
}

static void receivedNumbers(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 2)->value->uint16;
	numMaxNumbers = dict_find(data, 3)->value->uint16;
	for (int i = 0; i < 2; i++)
	{
		uint16_t groupPos = offset + i;

		if (groupPos >= numMaxNumbers)
			continue;

		setNumberType(groupPos, dict_find(data, 4 + i)->value->cstring);
		setNumber(groupPos, dict_find(data, 6 + i)->value->cstring);
	}

	menu_layer_reload_data(menuLayer);

	requestAdditionalNumbers();
}

void number_picker_window_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 0:
		receivedNumbers(data);
		break;

	}

}

void number_picker_window_data_sent(void)
{
	if (pickedNumber >= 0)
	{
		sendPickedNumber(pickedNumber);
	}
}

static void window_load(Window* me) {
	Layer* topLayer = window_get_root_layer(window);

	menuLayer = menu_layer_create(GRect(0, STATUSBAR_Y_OFFSET, 144, 168 - 16));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks){
		.get_num_sections = menu_get_num_sections_callback,
				.get_num_rows = menu_get_num_rows_callback,
				.get_cell_height = menu_get_row_height_callback,
				.draw_row = menu_draw_row_callback,
				.select_click = menu_select_callback,
				.selection_changed = menu_pos_changed
	});

	menu_layer_set_click_config_onto_window(menuLayer, window);

	layer_add_child(topLayer, (Layer*) menuLayer);

	#ifdef PBL_COLOR
		menu_layer_set_highlight_colors(menuLayer, GColorJaegerGreen, GColorBlack);
	#endif


#ifdef PBL_SDK_3
		statusBar = status_bar_layer_create();
		layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));
	#endif
}

static void window_appear(Window* me)
{
	setCurWindow(4);
}

static void window_unload(Window* me)
{
	menu_layer_destroy(menuLayer);

	#ifdef PBL_SDK_3
		status_bar_layer_destroy(statusBar);
	#endif

	window_destroy(me);
}

void number_picker_window_init(void)
{
	window = window_create();

	window_set_window_handlers(window, (WindowHandlers){
		.load = window_load,
		.appear = window_appear,
		.unload = window_unload
	});

	window_stack_push(window, true /* Animated */);
}

