#include "pebble.h"
#include "pebble_fonts.h"
#include "Dialer2.h"
#include "util.h"

Window* contactsWindow;

uint16_t numMaxContacts = 10;

int8_t arrayCenterPos = 0;
int16_t centerIndex = 0;

int16_t pickedContact = -1;

bool sending = false;
char cw_contactNames[21][21] = {};

MenuLayer* contactsMenuLayer;

int8_t cw_convertToArrayPos(uint16_t index)
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

char* cw_getContactName(uint16_t index)
{
	int8_t arrayPos = cw_convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return cw_contactNames[arrayPos];
}

void cw_setContactName(uint16_t index, char *name)
{
	int8_t arrayPos = cw_convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(cw_contactNames[arrayPos],name);
}

void cw_shiftContactArray(int newIndex)
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

	*cw_contactNames[clearIndex] = 0;
}

uint8_t cw_getEmptySpacesDown()
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i <= centerIndex + 10; i++)
	{
		if (i >= numMaxContacts)
			return 10;

		if (*cw_getContactName(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

uint8_t cw_getEmptySpacesUp()
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i >= centerIndex - 10; i--)
	{
		if (i < 0)
			return 10;

		if (*cw_getContactName(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

void contacts_requestContacts(uint16_t pos)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 5);
	dict_write_uint16(iterator, 1, pos);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);

	sending = true;
}

void contacts_sendPickedContact(int16_t pos)
{
	if (sending)
	{
		pickedContact = pos;
		return;
	}

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 6);
	dict_write_int16(iterator, 1, pos);
	app_message_outbox_send();

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void requestAdditionalContacts()
{
	if (sending)
		return;

	int emptyDown = cw_getEmptySpacesDown();
	int emptyUp = cw_getEmptySpacesUp();

	if (emptyDown < 6 && emptyDown <= emptyUp)
	{
		uint8_t startingIndex = centerIndex + emptyDown;
		contacts_requestContacts(startingIndex);
	}
	else if (emptyUp < 6)
	{
		uint8_t startingIndex = centerIndex - 2 - emptyUp;
		contacts_requestContacts(startingIndex);
	}
}

uint16_t cw_menu_get_num_sections_callback(MenuLayer *me, void *data) {
	return 1;
}

uint16_t cw_menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	return numMaxContacts;
}


int16_t cw_menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data) {
	return 27;
}

void cw_menu_pos_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
//	if (old_index.row == 0 && new_index.row != 1)
//		return;

	cw_shiftContactArray(new_index.row);
	requestAdditionalContacts();
}


void cw_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	graphics_context_set_text_color(ctx, GColorBlack);

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
		graphics_draw_text(ctx, cw_getContactName(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(3, 3, 141, 23), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}


void cw_menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	contacts_sendPickedContact(cell_index->row);
}

void contacts_receivedContactNames(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 1)->value->uint16;
	numMaxContacts = dict_find(data, 2)->value->uint16;
	for (int i = 0; i < 3; i++)
	{
		uint16_t groupPos = offset + i;
		if (groupPos >= numMaxContacts)
			break;


		cw_setContactName(groupPos, dict_find(data, 3 + i)->value->cstring);
	}

	menu_layer_reload_data(contactsMenuLayer);
	sending = false;

	if (pickedContact >= 0)
	{
		contacts_sendPickedContact(pickedContact);
		return;
	}
	requestAdditionalContacts();
}

void contacts_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 2:
		contacts_receivedContactNames(data);
		break;

	}

}

void contacts_window_load(Window *me) {
	setCurWindow(2);

	pickedContact = -1;
}

void contacts_window_unload(Window *me) {
	menu_layer_destroy(contactsMenuLayer);
	window_destroy(contactsWindow);
}

void init_contacts_window(char* names)
{
	contactsWindow = window_create();

	Layer* topLayer = window_get_root_layer(contactsWindow);

	contactsMenuLayer = menu_layer_create(GRect(0, 0, 144, 168 - 16));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(contactsMenuLayer, NULL, (MenuLayerCallbacks){
		.get_num_sections = cw_menu_get_num_sections_callback,
				.get_num_rows = cw_menu_get_num_rows_callback,
				.get_cell_height = cw_menu_get_row_height_callback,
				.draw_row = cw_menu_draw_row_callback,
				.select_click = cw_menu_select_callback,
				.selection_changed = cw_menu_pos_changed
	});

	if (names != NULL)
	{
		memcpy(cw_contactNames, names, 21 * 6);
		contacts_requestContacts(6);
	}

	menu_layer_set_click_config_onto_window(contactsMenuLayer, contactsWindow);

	layer_add_child(topLayer, (Layer*) contactsMenuLayer);

	window_set_window_handlers(contactsWindow, (WindowHandlers){
		.appear = contacts_window_load,
		.unload = contacts_window_unload,
	});

	window_stack_push(contactsWindow, names == NULL /* Animated */);
}

