#include "pebble.h"
#include "pebble_fonts.h"

#include "PebbleDialer.h"
#include "util.h"
#include "CircularBuffer.h"

static Window* window;

static uint16_t numMaxContacts = 7;

static int16_t pickedContact = -1;
static int8_t pickedFilterButton = -1;

static CircularBuffer* contacts;

static MenuLayer* contactsMenuLayer;

static StatusBarLayer* statusBar;

static bool filterMode;
static bool nothingFiltered;
static bool firstAppear;
static uint16_t scrollPosition;

static void menu_config_provider(void* context);

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
	uint16_t filledDown = cb_getNumOfLoadedSpacesDownFromCenter(contacts, numMaxContacts);
	uint16_t filledUp = cb_getNumOfLoadedSpacesUpFromCenter(contacts);

	if (filledDown < 6 && filledDown <= filledUp)
	{
		uint16_t startingIndex = contacts->centerIndex + filledDown;
		requestContacts(startingIndex);
	}
	else if (filledUp < 6)
	{
		uint16_t startingIndex = contacts->centerIndex - filledUp;
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
	cb_shift(contacts, new_index.row);
	requestAdditionalContacts();
}


static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	if (cell_index->section != 0)
			return;

	char* contact = cb_getEntry(contacts, cell_index->row);
	if (contact == NULL)
		return;

	if (menu_cell_layer_is_highlighted(cell_layer))
		graphics_context_set_text_color(ctx, GColorWhite);
	else
		graphics_context_set_text_color(ctx, GColorBlack);

	graphics_draw_text(ctx, contact, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, SCREEN_WIDTH - 3, 23), GTextOverflowModeTrailingEllipsis, PBL_IF_RECT_ELSE(GTextAlignmentLeft, GTextAlignmentCenter), NULL);
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

	cb_clear(contacts);
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

		char* contact = cb_getEntryForFilling(contacts, groupPos);
		if (contact != NULL)
			strcpy(contact, dict_find(data, 4 + i)->value->cstring);
	}

	if (numMaxContacts == 0)
	{
		cb_clear(contacts);
		char* contact = cb_getEntryForFilling(contacts, 0);
		if (contact != NULL)
			strcpy(contact, "No contacts");
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
	filterMode = true;
	nothingFiltered = true;
	firstAppear = true;
}

static void window_unload(Window* me)
{
	window_destroy(me);
}

static void window_appear(Window *me) {
	contacts = cb_create(sizeof(char) * 21, 13);

	Layer* topLayer = window_get_root_layer(window);

	contactsMenuLayer = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH, HEIGHT_BELOW_STATUSBAR));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(contactsMenuLayer, NULL, (MenuLayerCallbacks){
			.get_num_sections = menu_get_num_sections_callback,
			.get_num_rows = menu_get_num_rows_callback,
			.get_cell_height = menu_get_row_height_callback,
			.draw_row = menu_draw_row_callback,
			.selection_changed = menu_pos_changed
	});

	menu_layer_set_center_focused(contactsMenuLayer, false);

	layer_add_child(topLayer, (Layer*) contactsMenuLayer);

	if (filterMode)
	{
		window_set_click_config_provider(window, filter_config_provider);

		menu_layer_set_selected_index(contactsMenuLayer, MenuIndex(0, 0), MenuRowAlignCenter, false);
		menu_layer_set_selected_index(contactsMenuLayer, MenuIndex(-1, -1), MenuRowAlignNone, false);
	}
	else
	{
		contacts_window_stop_filtering();
	}

	#ifdef PBL_COLOR
		menu_layer_set_highlight_colors(contactsMenuLayer, GColorJaegerGreen, GColorBlack);
	#endif

	statusBar = status_bar_layer_create();
	layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));

	setCurWindow(3);

	if (firstAppear)
		firstAppear = false;
	else
	{
		menu_layer_set_selected_index(contactsMenuLayer, MenuIndex(0, scrollPosition), MenuRowAlignCenter, false);
		requestContacts(scrollPosition);
	}
}

static void window_disappear(Window* me)
{
	layer_remove_child_layers(window_get_root_layer(me));

	scrollPosition = menu_layer_get_selected_index(contactsMenuLayer).row;
	menu_layer_destroy(contactsMenuLayer);

	status_bar_layer_destroy(statusBar);

	cb_destroy(contacts);
}


void contacts_window_init()
{
	window = window_create();

	window_set_window_handlers(window, (WindowHandlers){
		.load = window_load,
		.unload = window_unload,
		.appear = window_appear,
		.disappear = window_disappear
	});

	window_stack_push(window, true /* Animated */);
}

