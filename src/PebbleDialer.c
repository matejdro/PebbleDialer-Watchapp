#include "pebble.h"
#include "pebble_fonts.h"

#include "CallLogWindow.h"
#include "ContactsWindow.h"
#include "CallWindow.h"
#include "MainMenuWindow.h"
#include "NumberPickerWindow.h"

const uint16_t PROTOCOL_VERSION = 6;

static uint8_t curWindow = 0;
static bool gotConfig = false;

bool config_dontClose;
bool config_noMenu;
uint8_t config_numOfGroups;
bool config_noFilterGroups;

bool closingMode;

void setCurWindow(int window)
{
	curWindow = window;
}

void switchWindow(int window)
{
	switch (window)
	{
	case 0:
		main_menu_init();
		break;
	case 1:
		call_window_init();
		break;
	case 2:
		call_log_window_init();
		break;
	case 3:
		contacts_window_init();
		break;
	case 4:
		number_picker_window_init();
		break;
	}
}

void closeApp(void)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 0);
	dict_write_uint8(iterator, 1, 2);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_message_outbox_send();

	closingMode = true;
}

static void received_config(DictionaryIterator *received)
{
	uint8_t* data = dict_find(received, 2)->value->data;

	uint16_t supportedVersion = (data[0] << 8) | (data[1]);
	if (supportedVersion > PROTOCOL_VERSION)
	{
		main_menu_show_old_watchapp();
		return;
	}
	else if (supportedVersion < PROTOCOL_VERSION)
	{
		main_menu_show_old_android();
		return;
	}

	config_noMenu = (data[2] & 0x01) != 0;
	config_dontClose = (data[2] & 0x02) != 0;
	config_noFilterGroups = (data[2] & 0x04) != 0;
	config_numOfGroups = data[3];

	gotConfig = true;

	if (!config_noMenu)
		main_menu_show_menu();

}

static void received_data(DictionaryIterator *received, void *context) {
	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

	uint8_t destModule = dict_find(received, 0)->value->uint8;
	uint8_t packetId = dict_find(received, 1)->value->uint8;
	bool autoSwitch = dict_find(received, 999) != NULL;

	if (destModule == 0 && packetId == 0)
	{
		received_config(received);
		return;
	}

	if (!gotConfig) //Do not react to anything until we got config
		return;

	if (destModule == 0)
	{
		if (packetId == 1)
		{
			closingMode = true;
			window_stack_pop_all(false);
			switchWindow(0);

			return;
		}
		if (curWindow != 0)
		{
			if (autoSwitch)
				switchWindow(0);
			else
				return;
		}

		main_menu_data_received(packetId, received);
	}
	else if (destModule == 1)
	{
		if (curWindow != 1)
		{
			if (autoSwitch)
				switchWindow(1);
			else
				return;
		}

		call_window_data_received(packetId, received);
	}
	else if (destModule == 2)
	{
		if (curWindow != 2)
		{
			if (autoSwitch)
				switchWindow(2);
			else
				return;
		}

		call_log_window_data_received(packetId, received);
	}
	else if (destModule == 3)
	{
		if (curWindow != 3)
		{
			if (autoSwitch)
				switchWindow(3);
			else
				return;
		}

		contacts_window_data_received(packetId, received);
	}
	else if (destModule == 4)
	{
		if (curWindow != 4)
		{
			if (autoSwitch)
				switchWindow(4);
			else
				return;
		}

		number_picker_window_data_received(packetId, received);
	}
}

static void data_delivered(DictionaryIterator *sent, void *context) {
	switch (curWindow)
	{
	case 2:
		call_log_window_data_sent();
		break;
	case 3:
		contacts_window_data_delivered();
		break;
	case 4:
		number_picker_window_data_sent();
		break;
	}
}

int main() {
	app_message_register_outbox_sent(data_delivered);
	app_message_register_inbox_received(received_data);

	app_message_open(124, 50);

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 0);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint16(iterator, 2, PROTOCOL_VERSION);
	#ifdef PBL_APLITE
		dict_write_uint8(iterator, 3, 0);
	#else
		dict_write_uint8(iterator, 3, 1);
	#endif

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_message_outbox_send();

	switchWindow(0);
	app_event_loop();
	window_stack_pop_all(false);
	return 0;
}
