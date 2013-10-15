#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "Dialer2.h"
#include "util.h"

Window numberPickerWindow;

uint16_t numMaxNumbers = 10;

int8_t np_arrayCenterPos = 0;
int16_t np_centerIndex = 0;

int16_t pickedNumber = -1;

bool np_sending = false;
char np_numberTypes[21][16] = {};
char np_numbers[21][16] = {};

MenuLayer numbersMenuLayer;

int8_t np_convertToArrayPos(uint16_t index)
{
	int16_t indexDiff = index - np_centerIndex;
	if (indexDiff > 10 || indexDiff < -10)
		return -1;

	int8_t arrayPos = np_arrayCenterPos + indexDiff;
	if (arrayPos < 0)
		arrayPos += 21;
	if (arrayPos > 20)
		arrayPos -= 21;

	return arrayPos;
}

char* np_getNumberType(uint16_t index)
{
	int8_t arrayPos = np_convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return np_numberTypes[arrayPos];
}

void np_setNumberType(uint16_t index, char *name)
{
	int8_t arrayPos = np_convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(np_numberTypes[arrayPos],name);
}

char* np_getNumber(uint16_t index)
{
	int8_t arrayPos = np_convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return np_numbers[arrayPos];
}

void np_setNumber(uint16_t index, char *name)
{
	int8_t arrayPos = np_convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(np_numbers[arrayPos],name);
}

void np_shiftArray(int newIndex)
{
	int8_t clearIndex;

	int16_t diff = newIndex - np_centerIndex;
	if (diff == 0)
		return;

	np_centerIndex+= diff;
	np_arrayCenterPos+= diff;

	if (diff > 0)
	{
		if (np_arrayCenterPos > 20)
			np_arrayCenterPos -= 21;

		clearIndex = np_arrayCenterPos - 10;
		if (clearIndex < 0)
			clearIndex += 21;
	}
	else
	{
		if (np_arrayCenterPos < 0)
			np_arrayCenterPos += 21;

		clearIndex = np_arrayCenterPos + 10;
		if (clearIndex > 20)
			clearIndex -= 21;
	}

	*np_numberTypes[clearIndex] = 0;
	*np_numbers[clearIndex] = 0;
}

uint8_t np_getEmptySpacesDown()
{
	uint8_t spaces = 0;
	for (int i = np_centerIndex; i <= np_centerIndex + 10; i++)
	{
		if (i >= numMaxNumbers)
			return 10;

		if (*np_getNumberType(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

uint8_t np_getEmptySpacesUp()
{
	uint8_t spaces = 0;
	for (int i = np_centerIndex; i >= np_centerIndex - 10; i--)
	{
		if (i < 0)
			return 10;

		if (*np_getNumberType(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

void np_requestNumbers(uint16_t pos)
{
	DictionaryIterator *iterator;
	app_message_out_get(&iterator);
	dict_write_uint8(iterator, 0, 8);
	dict_write_uint16(iterator, 1, pos);
	app_message_out_send();
	app_message_out_release();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);

	np_sending = true;
}

void np_sendPickedNumber(int16_t pos)
{
	if (np_sending)
	{
		pickedNumber = pos;
		return;
	}

	DictionaryIterator *iterator;
	app_message_out_get(&iterator);
	dict_write_uint8(iterator, 0, 9);
	dict_write_int16(iterator, 1, pos);
	app_message_out_send();
	app_message_out_release();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void requestAdditionalNumbers()
{
	if (np_sending)
		return;

	int emptyDown = np_getEmptySpacesDown();
	int emptyUp = np_getEmptySpacesUp();

	if (emptyDown < 6 && emptyDown <= emptyUp)
	{
		uint8_t startingIndex = np_centerIndex + emptyDown;
		np_requestNumbers(startingIndex);
	}
	else if (emptyUp < 6)
	{
		uint8_t startingIndex = np_centerIndex - 1 - emptyUp;
		np_requestNumbers(startingIndex);
	}
}

uint16_t np_menu_get_num_sections_callback(MenuLayer *me, void *data) {
	return 1;
}

uint16_t np_menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	return numMaxNumbers;
}


int16_t np_menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data) {
	return 44;
}

void np_menu_pos_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
//	if (old_index.row == 0 && new_index.row != 1)
//		return;

	np_shiftArray(new_index.row);
	requestAdditionalNumbers();
}


void np_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	//graphics_context_set_text_color(ctx, GColorBlack);

//	if (cell_index->row == 0)
//	{
//		graphics_text_draw(ctx, itoa(cw_getEmptySpacesUp()), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, 141, 23), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
//
//	}
//	else if (cell_index->row == 1)
//	{
//		graphics_text_draw(ctx, itoa(cw_getEmptySpacesDown()), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, 141, 23), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
//	}
//	if (cell_index->row == contactsMenuLayer.selection.index.row)
//		{
//			graphics_text_draw(ctx, itoa(centerIndex), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, 141, 23), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
//		}
//	else
	//	graphics_text_draw(ctx, np_getNumberType(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, 141, 23), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);


	menu_cell_basic_draw(ctx, cell_layer, np_getNumberType(cell_index->row), np_getNumber(cell_index->row), NULL);
}


void np_menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	np_sendPickedNumber(cell_index->row);
}

void np_receivedNumbers(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 1)->value->uint16;
	numMaxNumbers = dict_find(data, 2)->value->uint16;
	for (int i = 0; i < 2; i++)
	{
		uint16_t groupPos = offset + i;

		np_setNumberType(groupPos, dict_find(data, 3 + i)->value->cstring);
		np_setNumber(groupPos, dict_find(data, 5 + i)->value->cstring);
	}

	menu_layer_reload_data(&numbersMenuLayer);
	np_sending = false;

	if (pickedNumber >= 0)
	{
		np_sendPickedNumber(pickedNumber);
		return;
	}
	requestAdditionalNumbers();
}

void np_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 3:
		np_receivedNumbers(data);
		break;

	}

}

void np_window_load(Window *me) {
	//setCurWindow(3);

	//np_requestNumbers(0);
	pickedNumber = -1;
}

void init_number_picker_window()
{
	window_init(&numberPickerWindow, "Numbers");

	Layer* topLayer = window_get_root_layer(&numberPickerWindow);

	menu_layer_init(&numbersMenuLayer, GRect(0, 0, 144, 168 - 16));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(&numbersMenuLayer, NULL, (MenuLayerCallbacks){
		.get_num_sections = np_menu_get_num_sections_callback,
				.get_num_rows = np_menu_get_num_rows_callback,
				.get_cell_height = np_menu_get_row_height_callback,
				.draw_row = np_menu_draw_row_callback,
				.select_click = np_menu_select_callback,
				.selection_changed = np_menu_pos_changed
	});

	menu_layer_set_click_config_onto_window(&numbersMenuLayer, &numberPickerWindow);

	layer_add_child(topLayer, (Layer*) &numbersMenuLayer);

	window_set_window_handlers(&numberPickerWindow, (WindowHandlers){
		.appear = np_window_load
	});

	window_stack_push(&numberPickerWindow, true /* Animated */);
}

