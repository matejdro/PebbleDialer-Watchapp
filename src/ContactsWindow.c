#include "pebble.h"
#include "pebble_fonts.h"

#include "PebbleDialer.h"
#include "util.h"

static Window* window;

static uint16_t numMaxContacts = 7;

static int8_t arrayCenterPos = 0;
static int16_t centerIndex = 0;

static int16_t pickedContact = -1;
static int8_t pickedFilterButton = -1;

static char contactNames[21][21] = {};

static MenuLayer* contactsMenuLayer;

#ifdef PBL_SDK_3
	static StatusBarLayer* statusBar;
#endif

static bool filterMode;
static bool nothingFiltered;

static void menu_config_provider(void* context);

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

static char* getContactName(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return contactNames[arrayPos];
}

static void setContactName(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(contactNames[arrayPos],name);
}

static void shiftContactArray(int newIndex)
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

	*contactNames[clearIndex] = 0;
}

static uint8_t getEmptySpacesDown(void)
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i <= centerIndex + 10; i++)
	{
		if (i >= numMaxContacts)
			return 10;

		if (*getContactName(i) == 0)
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

		if (*getContactName(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

static void requestContacts(uint16_t pos)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 3);
	dict_write_uint8(iterator, 1, 1);
	dict_write_uint16(iterator, 2, pos);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
}

static void sendPickedContact(int16_t pos)
{
	DictionaryIterator *iterator;
	AppMessageResult result = app_message_outbox_begin(&iterator);
	if (result != APP_MSG_OK)
	{
			pickedContact = pos;
			return;
	}

	dict_write_uint8(iterator, 0, 3);
	dict_write_uint8(iterator, 1, 2);
	dict_write_int16(iterator, 2, pos);
	app_message_outbox_send();
	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

	pickedContact = -1;
}

static void requestAdditionalContacts(void)
{
	int emptyDown = getEmptySpacesDown();
	int emptyUp = getEmptySpacesUp();

	if (emptyDown < 6 && emptyDown <= emptyUp)
	{
		uint8_t startingIndex = centerIndex + emptyDown;
		requestContacts(startingIndex);
	}
	else if (emptyUp < 6)
	{
		uint8_t startingIndex = centerIndex - 2 - emptyUp;
		requestContacts(startingIndex);
	}
}

static uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
	if (filterMode)
		return 2; //Second section is dummy section to hold selection when filtering is active

	return 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	if (section_index != 0)
			return 1;

	return numMaxContacts;
}


static int16_t menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data) {
	if (cell_index->section != 0)
		return 0;

	return 27;
}

static void menu_pos_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
	shiftContactArray(new_index.row);
	requestAdditionalContacts();
}


static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	if (cell_index->section != 0)
			return;

	graphics_context_set_text_color(ctx, GColorBlack);
	graphics_draw_text(ctx, getContactName(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, 141, 23), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void filter(int button)
{
	DictionaryIterator *iterator;
	AppMessageResult result = app_message_outbox_begin(&iterator);
	if (result != APP_MSG_OK)
	{
			pickedFilterButton = button;
			return;
	}

	dict_write_uint8(iterator, 0, 3);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint8(iterator, 2, button);
	app_message_outbox_send();
	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

	memset(contactNames, 0, 21 * 21);
	nothingFiltered = button == 3;
	menu_layer_reload_data(contactsMenuLayer);
	pickedFilterButton = -1;
}


void contacts_window_stop_filtering(void)
{
	filterMode = false;
	menu_layer_set_selected_index(contactsMenuLayer, MenuIndex(0, 0), MenuRowAlignCenter, false);
	menu_layer_set_click_config_onto_window(contactsMenuLayer, window);

	window_set_click_config_provider(window, menu_config_provider);

}

static void filter_up_button(ClickRecognizerRef recognizer, Window *window) {
	filter(0);
}

static void filter_down_button(ClickRecognizerRef recognizer, Window *window) {

	filter(2);
}

static void filter_center_button(ClickRecognizerRef recognizer, Window *window)
{
	filter(1);
}

static void filter_back_button(ClickRecognizerRef recognizer, Window *window)
{
	if (nothingFiltered || !filterMode)
		window_stack_pop(true);
	else
		filter(3);
}

static void filter_center_long(ClickRecognizerRef recognizer, Window *window)
{
	requestContacts(8);
	contacts_window_stop_filtering();
}

static void menu_up_button(ClickRecognizerRef recognizer, Window *window) {
	menu_layer_set_selected_next(contactsMenuLayer, true, MenuRowAlignCenter, true);
}

static void menu_center_button(ClickRecognizerRef recognizer, Window *window)
{
	MenuIndex cell_index = menu_layer_get_selected_index(contactsMenuLayer);
	sendPickedContact(cell_index.row);

}


static void menu_down_button(ClickRecognizerRef recognizer, Window *window) {
	menu_layer_set_selected_next(contactsMenuLayer, false, MenuRowAlignCenter, true);
}

static void filter_config_provider(void* context) {
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) filter_up_button);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) filter_down_button);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) filter_center_button);
	window_single_click_subscribe(BUTTON_ID_BACK, (ClickHandler) filter_back_button);

	window_long_click_subscribe(BUTTON_ID_SELECT, 500, (ClickHandler) filter_center_long, NULL);
}

static void menu_config_provider(void* context)
{
	window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, (ClickHandler) menu_up_button);
	window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, (ClickHandler) menu_down_button);

	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) menu_center_button);
	window_single_click_subscribe(BUTTON_ID_BACK, (ClickHandler) filter_back_button);
}


static void receivedContactNames(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 2)->value->uint16;
	numMaxContacts = dict_find(data, 3)->value->uint16;
	for (int i = 0; i < 3; i++)
	{
		uint16_t groupPos = offset + i;
		if (groupPos >= numMaxContacts)
			break;


		setContactName(groupPos, dict_find(data, 4 + i)->value->cstring);
	}

	if (numMaxContacts == 0)
	{
		setContactName(0, "No contacts");
		numMaxContacts = 1;
	}

	menu_layer_reload_data(contactsMenuLayer);

	requestAdditionalContacts();
}

void contacts_window_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 0:
		receivedContactNames(data);
		break;

	}
}

void contacts_window_data_delivered(void)
{
	if (pickedContact != -1)
		sendPickedContact(pickedContact);
}

static void window_load(Window* me)
{
	Layer* topLayer = window_get_root_layer(window);

	contactsMenuLayer = menu_layer_create(GRect(0, STATUSBAR_Y_OFFSET, 144, 168 - 16));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(contactsMenuLayer, NULL, (MenuLayerCallbacks){
		.get_num_sections = menu_get_num_sections_callback,
		.get_num_rows = menu_get_num_rows_callback,
		.get_cell_height = menu_get_row_height_callback,
		.draw_row = menu_draw_row_callback,
		.selection_changed = menu_pos_changed
	});

	layer_add_child(topLayer, (Layer*) contactsMenuLayer);

	window_set_click_config_provider(window, filter_config_provider);

	filterMode = true;
	nothingFiltered = true;
	menu_layer_set_selected_index(contactsMenuLayer, MenuIndex(0, 0), MenuRowAlignCenter, false);
	menu_layer_set_selected_index(contactsMenuLayer, MenuIndex(-1, -1), MenuRowAlignNone, false);
	centerIndex = 0;
	arrayCenterPos = 0;

	memset(contactNames, 0, 21 * 21);

	#ifdef PBL_COLOR
		menu_layer_set_highlight_colors(contactsMenuLayer, GColorJaegerGreen, GColorBlack);
	#endif

	#ifdef PBL_SDK_3
		statusBar = status_bar_layer_create();
		layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));
	#endif
}

static void window_appear(Window *me) {
	setCurWindow(3);
}

static void window_unload(Window* me)
{
	menu_layer_destroy(contactsMenuLayer);

	#ifdef PBL_SDK_3
		status_bar_layer_destroy(statusBar);
	#endif

	window_destroy(me);
}

void contacts_window_init()
{
	window = window_create();

	window_set_window_handlers(window, (WindowHandlers){
		.load = window_load,
		.unload = window_unload,
		.appear = window_appear
	});

	window_stack_push(window, true /* Animated */);
}

