#include "pebble.h"
#include "pebble_fonts.h"
#include "Dialer2.h"
#include "ContactsWindow.h"

Window* filterWindow;

int numOfContacts = 0;

char contactNames[7][21] = {};

TextLayer* filterLoadingLayer;
MenuLayer* filterMenuLayer;

bool loading = true;

uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
	return 1;
}

uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	return 7;
}


int16_t menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data) {
	return 27;
}



void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	graphics_context_set_text_color(ctx, GColorBlack);
	graphics_draw_text(ctx, contactNames[cell_index->row], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, 141, 23), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}


void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
}


void filter_show_loading()
{
	layer_set_hidden((Layer *) filterLoadingLayer, false);
	layer_set_hidden((Layer *) filterMenuLayer, true);
	loading = true;
}

void filter_show_menu()
{
	MenuIndex invalidIndex;
	invalidIndex.row = -1;
	invalidIndex.section = -1;
	menu_layer_set_selected_index(filterMenuLayer, invalidIndex, MenuRowAlignNone, false);
	layer_set_hidden((Layer *) filterLoadingLayer, true);
	layer_set_hidden((Layer *) filterMenuLayer, false);
	loading = false;
}

void filter_requestContacts(uint8_t pos)
{
	if (pos >= numOfContacts)
	{
		return;
	}

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 3);
	dict_write_uint8(iterator, 1, pos);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void filter_receivedContactNames(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 1)->value->uint16;
	numOfContacts = dict_find(data, 2)->value->uint16;
	for (int i = 0; i < 3; i++)
	{
		int groupPos = offset + i;
		if (groupPos >= numOfContacts)
			break;

		strcpy(contactNames[groupPos], dict_find(data, 3 + i)->value->cstring);
	}
	filter_requestContacts(offset + 3);

	menu_layer_reload_data(filterMenuLayer);

	if (loading)
		filter_show_menu();
}

void filter_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 2:
		filter_receivedContactNames(data);
		break;

	}

}

void filter(int button)
{
	if (loading)
		return;

	filter_show_loading();

	memset(contactNames, 0, 20 * 7);

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);

	dict_write_uint8(iterator, 0, 4);
	dict_write_uint8(iterator, 1, button);

	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void filter_up_button(ClickRecognizerRef recognizer, Window *window) {
	filter(0);
}

void filter_down_button(ClickRecognizerRef recognizer, Window *window) {

	filter(2);
}

void filter_center_button(ClickRecognizerRef recognizer, Window *window)
{
	filter(1);
}



void filter_center_long(ClickRecognizerRef recognizer, Window *window)
{
	init_contacts_window(contactNames[0]);
}

void filter_dummy_release_handler(ClickRecognizerRef recognizer, Window *window)
{

}


void filter_config_provider(void* context) {
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) filter_up_button);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) filter_down_button);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) filter_center_button);

	window_long_click_subscribe(BUTTON_ID_SELECT, 500, (ClickHandler) filter_center_long, (ClickHandler) filter_dummy_release_handler);
}

void filter_window_load(Window *me) {
	setCurWindow(1);

	filter_show_loading();

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 3);
	dict_write_uint8(iterator, 1, 0);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void init_filter_window()
{
	filterWindow = window_create();

	Layer* topLayer = window_get_root_layer(filterWindow);

	filterLoadingLayer = text_layer_create(GRect(0, 10, 144, 168 - 16));
	text_layer_set_text_alignment(filterLoadingLayer, GTextAlignmentCenter);
	text_layer_set_text(filterLoadingLayer, "Loading...");
	text_layer_set_font(filterLoadingLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, (Layer*) filterLoadingLayer);

	window_set_click_config_provider(filterWindow, (ClickConfigProvider) filter_config_provider);

	filterMenuLayer = menu_layer_create(GRect(0, 0, 144, 168 - 16));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(filterMenuLayer, NULL, (MenuLayerCallbacks){
		.get_num_sections = menu_get_num_sections_callback,
				.get_num_rows = menu_get_num_rows_callback,
				.get_cell_height = menu_get_row_height_callback,
				.draw_row = menu_draw_row_callback,
				.select_click = menu_select_callback,
	});

	layer_add_child(topLayer, (Layer*) filterMenuLayer);

	window_set_window_handlers(filterWindow, (WindowHandlers){
		.appear = filter_window_load,
	});

	window_stack_push(filterWindow, true /* Animated */);
}

