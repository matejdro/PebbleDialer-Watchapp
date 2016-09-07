#include "pebble.h"
#include "pebble_fonts.h"
#include <pebble-activity-indicator-layer/activity-indicator-layer.h>

#include "PebbleDialer.h"
#include "ContactsWindow.h"
#include "MainMenuWindow.h"

static Window* window = NULL;

static char groupNames[20][21] = {};

static GBitmap* callHistoryIcon;
static GBitmap* contactsIcon;
static GBitmap* contactGroupIcon;

static TextLayer* loadingLayer;

#ifndef PBL_LOW_MEMORY
    static const uint16_t ACTIVITY_INDICATOR_SIZE = 50;
    static const uint16_t ACTIVITY_INDICATOR_THICKNESS = 5;
    static ActivityIndicatorLayer *loadingIndicator;
#endif

static TextLayer* quitTitle;
static TextLayer* quitText;

static MenuLayer* menuLayer;

static StatusBarLayer* statusBar;

static bool menuLoaded = false;

static void show_error_base(void);

static void show_loading(void)
{
	layer_set_hidden((Layer *) loadingLayer, false);
	layer_set_hidden((Layer *) quitTitle, true);
	layer_set_hidden((Layer *) quitText, true);
	layer_set_hidden((Layer *) menuLayer, true);

#ifndef PBL_LOW_MEMORY
    activity_indicator_layer_set_animating(loadingIndicator, true);
    layer_set_hidden(activity_indicator_layer_get_layer(loadingIndicator), false);
#endif
}

void main_menu_show_closing(void)
{
	if (!window_stack_contains_window(window))
		main_menu_init();
	layer_set_hidden((Layer *) loadingLayer, true);
	layer_set_hidden((Layer *) menuLayer, true);
	layer_set_hidden((Layer *) quitTitle, false);
	layer_set_hidden((Layer *) quitText, false);

	closingMode = true;
}

void main_menu_show_old_watchapp(void)
{
    show_error_base();
	text_layer_set_text(loadingLayer, "Dialer\nOutdated Watchapp \n\n Check your phone");

}

void main_menu_show_old_android(void)
{
    show_error_base();
	text_layer_set_text(loadingLayer, "Dialer\nUpdate Android App \n\n Open link:\n bit.ly/1KhZCog");
}

void main_menu_show_no_connection(void)
{
    if (window == NULL)
        return;

    show_error_base();
    text_layer_set_text(loadingLayer, "Dialer\n\nPhone is not connected.");
}

static void show_error_base(void)
{
	layer_set_hidden((Layer *) loadingLayer, false);
	layer_set_hidden((Layer *) quitTitle, true);
	layer_set_hidden((Layer *) quitText, true);
	layer_set_hidden((Layer *) menuLayer, true);

#ifndef PBL_LOW_MEMORY
    activity_indicator_layer_set_animating(loadingIndicator, false);
    layer_set_hidden(activity_indicator_layer_get_layer(loadingIndicator), true);
#endif

	text_layer_set_font(loadingLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
}

static uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
	return 1;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	return config_numOfGroups + 2;
}

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	char* text;
	GBitmap* icon;

	int16_t index = cell_index->row;

	if (index == 0)
	{
		text = "Call History";
		icon = callHistoryIcon;
	}
	else if (index == 1)
	{
		text = "All contacts";
		icon = contactsIcon;
	}
	else
	{
		text = groupNames[index - 2];
		icon = contactGroupIcon;
	}


	graphics_context_set_compositing_mode(ctx, GCompOpSet);
	menu_cell_basic_draw(ctx, cell_layer, text, NULL, icon);
}

void main_menu_show_menu(void)
{
	menuLoaded = true;

	layer_set_hidden((Layer *) loadingLayer, true);
	layer_set_hidden((Layer *) menuLayer, false);
	layer_set_hidden((Layer *) quitTitle, true);
	layer_set_hidden((Layer *) quitText, true);

#ifndef PBL_LOW_MEMORY
     activity_indicator_layer_set_animating(loadingIndicator, false);
     layer_set_hidden(activity_indicator_layer_get_layer(loadingIndicator), true);
#endif
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

static void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);

	int16_t index = cell_index->row;

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
	}

	menu_layer_reload_data(menuLayer);
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
		main_menu_show_closing();
		closeApp();
		app_timer_register(3000, closing_timer, NULL);
	}
	else if (menuLoaded)
	{
		main_menu_show_menu();
	}
    else if (!connection_service_peek_pebble_app_connection())
    {
        main_menu_show_no_connection();
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
	loadingLayer = text_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH, HEIGHT_BELOW_STATUSBAR));
	text_layer_set_text_alignment(loadingLayer, GTextAlignmentCenter);
	text_layer_set_text(loadingLayer, "Dialer");
	text_layer_set_font(loadingLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, (Layer*) loadingLayer);

	quitTitle = text_layer_create(GRect(0, 70 + STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH, 50));
	text_layer_set_text_alignment(quitTitle, GTextAlignmentCenter);
	text_layer_set_text(quitTitle, "Press back again if app does not close in several seconds");
	layer_add_child(topLayer, (Layer*) quitTitle);

	quitText = text_layer_create(GRect(0, 10 + STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH, 50));
	text_layer_set_text_alignment(quitText, GTextAlignmentCenter);
	text_layer_set_text(quitText, "Quitting...\n Please wait");
	text_layer_set_font(quitText, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, (Layer*) quitText);

#ifndef PBL_LOW_MEMORY
    loadingIndicator = activity_indicator_layer_create(GRect(SCREEN_WIDTH / 2 - ACTIVITY_INDICATOR_SIZE / 2, HEIGHT_BELOW_STATUSBAR / 2 - ACTIVITY_INDICATOR_SIZE / 2 + STATUS_BAR_LAYER_HEIGHT, ACTIVITY_INDICATOR_SIZE, ACTIVITY_INDICATOR_SIZE));
    activity_indicator_layer_set_thickness(loadingIndicator, ACTIVITY_INDICATOR_THICKNESS);
    layer_add_child(topLayer, activity_indicator_layer_get_layer(loadingIndicator));

    text_layer_set_text(loadingLayer, "Dialer");
#else
    text_layer_set_text(loadingLayer, "Dialer\n\nLoading");
#endif

    menuLayer = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH, HEIGHT_BELOW_STATUSBAR));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks){
			.get_num_sections = menu_get_num_sections_callback,
			.get_num_rows = menu_get_num_rows_callback,
			.draw_row = menu_draw_row_callback,
			.select_click = menu_select_callback,
	});

	#ifdef PBL_COLOR
		menu_layer_set_highlight_colors(menuLayer, GColorJaegerGreen, GColorBlack);
	#endif

	menu_layer_set_click_config_onto_window(menuLayer, window);
	layer_add_child(topLayer, (Layer*) menuLayer);

	statusBar = status_bar_layer_create();
	layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));
}

static void window_unload(Window* me)
{
	gbitmap_destroy(callHistoryIcon);
	gbitmap_destroy(contactsIcon);
	gbitmap_destroy(contactGroupIcon);

	text_layer_destroy(loadingLayer);
	text_layer_destroy(quitTitle);
	text_layer_destroy(quitText);

	menu_layer_destroy(menuLayer);
	status_bar_layer_destroy(statusBar);
#ifndef PBL_LOW_MEMORY
    activity_indicator_layer_destroy(loadingIndicator);
#endif
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

