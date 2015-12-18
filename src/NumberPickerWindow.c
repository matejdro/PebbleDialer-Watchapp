#include <pebble.h>
#include "pebble.h"
#include "pebble_fonts.h"

#include "PebbleDialer.h"
#include "util.h"
#include "ActionsMenu.h"
#include "CircularBuffer.h"

static Window* window;

static uint16_t numMaxNumbers = 10;

static int16_t pickedNumber = -1;

static GBitmap* callIcon;
static GBitmap* messageIcon;

typedef struct
{
	uint8_t actionType;
	char numberType[21];
	char number[21];

} ContactAction;

static CircularBuffer* contactActions;

MenuLayer* menuLayer;

StatusBarLayer* statusBar;

static void sendMenuPick(uint8_t buttonId);
static void sendPickedNumber(int16_t pos);

static void button_up_press(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		actions_menu_move_up();
		return;
	}

	menu_layer_set_selected_next(menuLayer, true, MenuRowAlignCenter, true);
}

static void button_select_press(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		sendMenuPick(actions_menu_get_selected_index());
		actions_menu_hide();
		return;
	}

	sendPickedNumber(menu_layer_get_selected_index(menuLayer).row);
}

static void button_down_press(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		actions_menu_move_down();
		return;
	}

	menu_layer_set_selected_next(menuLayer, false, MenuRowAlignCenter, false);
}

static void sendMenuPick(uint8_t buttonId)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);

	dict_write_uint8(iterator, 0, 5);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint8(iterator, 2, buttonId);

	app_message_outbox_send();
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
	uint8_t filledDown = cb_getNumOfLoadedSpacesDownFromCenter(contactActions, numMaxNumbers);
	uint8_t filledUp = cb_getNumOfLoadedSpacesUpFromCenter(contactActions);

	if (filledDown < 6 && filledDown <= filledUp)
	{
		uint16_t startingIndex = contactActions->centerIndex + filledDown;
		requestNumbers(startingIndex);
	}
	else if (filledUp < 6)
	{
		uint16_t startingIndex = contactActions->centerIndex - 1 - filledUp;
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
	cb_shift(contactActions, new_index.row);
	requestAdditionalNumbers();
}


static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	ContactAction* action = cb_getEntry(contactActions, cell_index->row);
	if (action == NULL)
		return;

	GBitmap* icon = action->actionType == 0 ? callIcon : messageIcon;

#ifdef PBL_ROUND
	char title[32];
	strncpy(title, icon == callIcon ? "[Call] " : "[Msg] ", sizeof title - 1);
	strncat(title, action->numberType, sizeof title - 1 - strlen(title));
	menu_cell_basic_draw(ctx, cell_layer, title, action->number, icon);
#else
	menu_cell_basic_draw(ctx, cell_layer, action->numberType, action->number, icon);
#endif
}


static void receivedNumbers(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 2)->value->uint16;
	numMaxNumbers = dict_find(data, 3)->value->uint16;
	uint8_t* actions = dict_find(data, 8)->value->data;
	for (int i = 0; i < 2; i++)
	{
		uint16_t groupPos = offset + i;

		if (groupPos >= numMaxNumbers)
			continue;

		ContactAction* action = cb_getEntryForFilling(contactActions, groupPos);
		if (action == NULL)
			continue;

		strcpy(action->numberType, dict_find(data, 4 + i)->value->cstring);
		strcpy(action->number, dict_find(data, 6 + i)->value->cstring);
		action->actionType = actions[i];
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

static void config_provider(void* context) {
	window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, (ClickHandler) button_up_press);
	window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, (ClickHandler) button_down_press);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) button_select_press);
}


static void window_load(Window* me) {
	contactActions = cb_create(sizeof(ContactAction), 8);

	Layer* topLayer = window_get_root_layer(window);

	menuLayer = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH, HEIGHT_BELOW_STATUSBAR));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks){
		.get_num_sections = menu_get_num_sections_callback,
				.get_num_rows = menu_get_num_rows_callback,
				.get_cell_height = menu_get_row_height_callback,
				.draw_row = menu_draw_row_callback,
				.selection_changed = menu_pos_changed
	});

	menu_layer_set_click_config_onto_window(menuLayer, window);
	window_set_click_config_provider(window, config_provider);

	layer_add_child(topLayer, (Layer*) menuLayer);

	#ifdef PBL_COLOR
		menu_layer_set_highlight_colors(menuLayer, GColorJaegerGreen, GColorBlack);
	#endif

	statusBar = status_bar_layer_create();
	layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));

	callIcon = gbitmap_create_with_resource(RESOURCE_ID_ICON);
	messageIcon = gbitmap_create_with_resource(RESOURCE_ID_MESSAGE);

	actions_menu_init();
	actions_menu_attach(topLayer);
}

static void window_appear(Window* me)
{
	setCurWindow(4);
}

static void window_unload(Window* me)
{
	menu_layer_destroy(menuLayer);
	status_bar_layer_destroy(statusBar);

	gbitmap_destroy(callIcon);
	gbitmap_destroy(messageIcon);

	actions_menu_deinit();

	window_destroy(me);
	cb_destroy(contactActions);
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

