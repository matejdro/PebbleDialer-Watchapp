#include "pebble.h"
#include "pebble_fonts.h"

#include "PebbleDialer.h"
#include "ContactsWindow.h"

static Window* window;

static char groupNames[20][21] = {};
static SimpleMenuItem mainMenuItems[22] = {};
static SimpleMenuSection mainMenuSection[1] = {};

static GBitmap* callHistoryIcon;
static GBitmap* contactsIcon;
static GBitmap* contactGroupIcon;

static TextLayer* loadingLayer;

static TextLayer* quitTitle;
static TextLayer* quitText;

static SimpleMenuLayer* menuLayer;

static bool menuLoaded = false;

static void show_update_dialog(void);
static void reload_menu(void);
static void menu_picked(int index, void* context);

static void show_loading(void)
{
	layer_set_hidden((Layer *) loadingLayer, false);
	layer_set_hidden((Layer *) quitTitle, true);
	layer_set_hidden((Layer *) quitText, true);
	if (menuLayer != NULL) layer_set_hidden((Layer *) menuLayer, true);
}

void main_menu_show_closing(void)
{
	layer_set_hidden((Layer *) loadingLayer, true);
	if (menuLayer != NULL) layer_set_hidden((Layer *) menuLayer, true);
	layer_set_hidden((Layer *) quitTitle, false);
	layer_set_hidden((Layer *) quitText, false);

	closingMode = true;
}

void main_menu_show_old_watchapp(void)
{
	show_update_dialog();
	text_layer_set_text(loadingLayer, "Pebble Dialer\nOutdated Watchapp \n\n Check your phone");

}

void main_menu_show_old_android(void)
{
	show_update_dialog();
	text_layer_set_text(loadingLayer, "Pebble Dialer\nUpdate Android App \n\n Open link:\n NO_LINK_YET");
}

static void show_update_dialog(void)
{
	layer_set_hidden((Layer *) loadingLayer, false);
	layer_set_hidden((Layer *) quitTitle, true);
	layer_set_hidden((Layer *) quitText, true);
	if (menuLayer != NULL) layer_set_hidden((Layer *) menuLayer, true);

	text_layer_set_font(loadingLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
}

void main_menu_show_menu(void)
{
	menuLoaded = true;

	mainMenuSection[0].items = mainMenuItems;
	mainMenuSection[0].num_items = config_numOfGroups + 2;

	mainMenuItems[0].title = "Call History";
	mainMenuItems[0].icon = callHistoryIcon;
	mainMenuItems[0].callback = menu_picked;

	mainMenuItems[1].title = "All contacts";
	mainMenuItems[1].icon = contactsIcon;
	mainMenuItems[1].callback = menu_picked;

	for (int i = 0; i < config_numOfGroups; i++)
	{
		mainMenuItems[i + 2].title = groupNames[i];
		mainMenuItems[i + 2].icon = contactGroupIcon;
		mainMenuItems[i + 2].callback = menu_picked;
	}

	reload_menu();

	layer_set_hidden((Layer *) loadingLayer, true);
	layer_set_hidden((Layer *) menuLayer, false);
	layer_set_hidden((Layer *) quitTitle, true);
	layer_set_hidden((Layer *) quitText, true);
}


static void reload_menu(void)
{
	Layer* topLayer = window_get_root_layer(window);

	if (menuLayer != NULL)
	{
		layer_remove_from_parent((Layer *) menuLayer);
		simple_menu_layer_destroy(menuLayer);
	}

	menuLayer = simple_menu_layer_create(GRect(0, 0, 144, 152), window, mainMenuSection, 1, NULL);

	layer_add_child(topLayer, (Layer *) menuLayer);
}

void main_menu_close(void)
{
	if (window != NULL)
	{
		window_stack_remove(window, false);
		window = NULL;
	}
}

static void closing_timer(void* data)
{
	if (!closingMode)
		return;

	closeApp();
	app_timer_register(3000, closing_timer, NULL);
}

static void menu_picked(int index, void* context)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);

	dict_write_uint8(iterator, 0, 0);
	dict_write_uint8(iterator, 1, 1);
	dict_write_uint8(iterator, 2, index);

	app_message_outbox_send();

	int newWindow = index > 0 ? 3 : 2;
	switchWindow(newWindow);
	if (index > 1 && config_noFilterGroups)
		contacts_window_stop_filtering();
}


static void receivedGroupNames(DictionaryIterator* data)
{
	uint8_t offset = dict_find(data, 2)->value->uint8;

	for (int i = 0; i < 3; i++)
	{

		int groupPos = offset + i;
		if (groupPos >= config_numOfGroups)
			break;

		strcpy(groupNames[groupPos], dict_find(data, 3 + i)->value->cstring);
		mainMenuItems[groupPos + 2].title = groupNames[groupPos];
	}

	menu_layer_reload_data(simple_menu_layer_get_menu_layer(menuLayer));
}

void main_menu_data_received(int packetId, DictionaryIterator* data)
{

	switch (packetId)
	{
	case 2:
		receivedGroupNames(data);
		break;

	}

}

static void window_appears(Window *me)
{
	if (closingMode)
	{
		closeApp();
		app_timer_register(3000, closing_timer, NULL);
	}
	else if (menuLoaded)
	{
		main_menu_show_menu();
	}
	else
	{
		show_loading();
	}

	setCurWindow(0);
}

static void window_load(Window *me) {
	callHistoryIcon = gbitmap_create_with_resource(RESOURCE_ID_CALL_HISTORY);
	contactsIcon = gbitmap_create_with_resource(RESOURCE_ID_CONTACTS);
	contactGroupIcon = gbitmap_create_with_resource(RESOURCE_ID_CONTACT_GROUP);

	Layer* topLayer = window_get_root_layer(me);

	loadingLayer = text_layer_create(GRect(0, 0, 144, 168 - 16));
	text_layer_set_text_alignment(loadingLayer, GTextAlignmentCenter);
	text_layer_set_text(loadingLayer, "Loading...");
	text_layer_set_font(loadingLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, (Layer*) loadingLayer);

	quitTitle = text_layer_create(GRect(0, 70, 144, 50));
	text_layer_set_text_alignment(quitTitle, GTextAlignmentCenter);
	text_layer_set_text(quitTitle, "Press back again if app does not close in several seconds");
	layer_add_child(topLayer, (Layer*) quitTitle);

	quitText = text_layer_create(GRect(0, 10, 144, 50));
	text_layer_set_text_alignment(quitText, GTextAlignmentCenter);
	text_layer_set_text(quitText, "Quitting...\n Please wait");
	text_layer_set_font(quitText, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, (Layer*) quitText);
}

static void window_unload(Window* me)
{
	gbitmap_destroy(callHistoryIcon);
	gbitmap_destroy(contactsIcon);
	gbitmap_destroy(contactGroupIcon);

	text_layer_destroy(loadingLayer);
	text_layer_destroy(quitTitle);
	text_layer_destroy(quitText);

	window_destroy(me);
}

void main_menu_init(void)
{
	window = window_create();

	window_set_window_handlers(window, (WindowHandlers){
		.appear = window_appears,
		.load = window_load,
		.unload = window_unload
	});

	window_stack_push(window, true /* Animated */);
	setCurWindow(0);
}

