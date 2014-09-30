#include "pebble.h"
#include "pebble_fonts.h"
#include "Dialer2.h"

Window* menuWindow;

int numOfGroups = 0;

char MainMenuGroupNames[20][21] = {};
SimpleMenuItem mainMenuItems[22] = {};
SimpleMenuSection mainMenuSection[1] = {};

GBitmap* callHistoryIcon;
GBitmap* contactsIcon;
GBitmap* contactGroupIcon;

TextLayer* menuLoadingLayer;
SimpleMenuLayer* menuLayer = NULL;

bool skipGroupFiltering;

void show_loading()
{
	layer_set_hidden((Layer *) menuLoadingLayer, false);
	if (menuLayer != NULL) layer_set_hidden((Layer *) menuLayer, true);
}

void menu_picked(int index, void* context)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);

	dict_write_uint8(iterator, 0, 2);
	dict_write_uint8(iterator, 1, index);

	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);

	int newWindow = index > 0 ? 1 : 4;
	if (index > 1 && skipGroupFiltering)
		newWindow = 2;
	switchWindow(newWindow);
}

void show_menu()
{
	mainMenuSection[0].items = mainMenuItems;
	mainMenuSection[0].num_items = numOfGroups + 2;

	mainMenuItems[0].title = "Call History";
	mainMenuItems[0].icon = callHistoryIcon;
	mainMenuItems[0].callback = menu_picked;

	mainMenuItems[1].title = "All contacts";
	mainMenuItems[1].icon = contactsIcon;
	mainMenuItems[1].callback = menu_picked;

	for (int i = 0; i < numOfGroups; i++)
	{
		mainMenuItems[i + 2].title = MainMenuGroupNames[i];
		mainMenuItems[i + 2].icon = contactGroupIcon;
		mainMenuItems[i + 2].callback = menu_picked;

	}

	Layer* topLayer = window_get_root_layer(menuWindow);

	if (menuLayer != NULL)
	{
                layer_remove_from_parent((Layer *) menuLayer);
                simple_menu_layer_destroy(menuLayer);
        }
	menuLayer = simple_menu_layer_create(GRect(0, 0, 144, 156), menuWindow, mainMenuSection, 1, NULL);
	layer_add_child(topLayer, (Layer *) menuLayer);

	layer_set_hidden((Layer *) menuLoadingLayer, true);
	layer_set_hidden((Layer *) menuLayer, false);
}

void request_categories(uint8_t pos)
{
	if (pos >= numOfGroups)
	{
		show_menu();
		return;
	}

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 1);
	dict_write_uint8(iterator, 1, pos);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void receivedGroupNames(DictionaryIterator* data)
{
	uint8_t offset = dict_find(data, 1)->value->uint8;
	for (int i = 0; i < 3; i++)
	{

		int groupPos = offset + i;
		if (groupPos >= numOfGroups)
			break;

		strcpy(MainMenuGroupNames[groupPos], dict_find(data, 2 + i)->value->cstring);
	}

	request_categories(offset + 3);
}

void menu_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 0:
		numOfGroups = dict_find(data, 1)->value->uint8;
		skipGroupFiltering = dict_find(data, 2)->value->uint8 == 1;
		request_categories(0);
		break;
	case 1:
		receivedGroupNames(data);
		break;

	}

}

void window_load(Window *me) {
	setCurWindow(0);

	show_loading();

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 0);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void window_unload(Window* me)
{
	gbitmap_destroy(callHistoryIcon);
	gbitmap_destroy(contactsIcon);
	gbitmap_destroy(contactGroupIcon);

	window_destroy(menuWindow);
	text_layer_destroy(menuLoadingLayer);
}

void init_menu_window()
{
	callHistoryIcon = gbitmap_create_with_resource(RESOURCE_ID_CALL_HISTORY);
	contactsIcon = gbitmap_create_with_resource(RESOURCE_ID_CONTACTS);
	contactGroupIcon = gbitmap_create_with_resource(RESOURCE_ID_CONTACT_GROUP);

	menuWindow = window_create();

	Layer* topLayer = window_get_root_layer(menuWindow);

	menuLoadingLayer = text_layer_create(GRect(0, 10, 144, 168 - 16));
	text_layer_set_text_alignment(menuLoadingLayer, GTextAlignmentCenter);
	text_layer_set_text(menuLoadingLayer, "Loading...");
	text_layer_set_font(menuLoadingLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, (Layer*) menuLoadingLayer);


	window_set_window_handlers(menuWindow, (WindowHandlers){
		.appear = window_load,
		.unload = window_unload
	});

	window_stack_push(menuWindow, true /* Animated */);
}

