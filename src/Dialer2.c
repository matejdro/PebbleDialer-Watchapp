#include "pebble.h"
#include "pebble_fonts.h"
#include "DialerMenu.h"
#include "FilterWindow.h"
#include "ContactsWindow.h"
#include "callscreen.h"
#include "NumberPicker.h"
#include "CallLog.h"

uint8_t curWindow = 0;

void setCurWindow(int window)
{
	curWindow = window;
}

void switchWindow(int window)
{
	switch (window)
	{
	case 0:
		init_menu_window();
		curWindow = 0;
		break;
	case 1:
		init_filter_window();
		curWindow = 1;
		break;
	case 2:
		init_contacts_window(NULL);
		curWindow = 2;
		break;
	case 3:
		init_number_picker_window();
		curWindow = 3;
		break;
	case 4:
		init_call_log_window();
		curWindow = 4;
		break;
	case 10:
		callscreen_init();
		curWindow = 10;
		break;
	}
}

void received_data(DictionaryIterator *received, void *context) {
	uint8_t packetId = dict_find(received, 0)->value->uint8;

	if (packetId == 5 && curWindow != 10)
	{
		if (curWindow == 0)
			window_stack_pop(false);

		switchWindow(10);
	}
	else if (packetId == 3 && curWindow != 3)
	{
		switchWindow(3);
	}

	switch (curWindow)
	{
	case 0:
		menu_data_received(packetId, received);
		break;
	case 1:
		filter_data_received(packetId, received);
		break;
	case 2:
		contacts_data_received(packetId, received);
		break;
	case 3:
		np_data_received(packetId, received);
		break;
	case 4:
		cl_data_received(packetId, received);
		break;

	case 10:
		callscreen_received_data(packetId, received);
		break;

	}

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void data_delivered(DictionaryIterator *sent, void *context) {
	if (curWindow == 10)
	{
		callscreen_data_delivered(sent);
	}
}

void second_tick()
{
	if (curWindow == 10)
	{
		callscreen_second();
	}
}

int main() {
	app_message_register_outbox_sent(data_delivered);
	app_message_register_inbox_received(received_data);
	app_message_open(124, 50);

	tick_timer_service_subscribe(SECOND_UNIT, (TickHandler) second_tick);
	init_menu_window();
	app_event_loop();
	return 0;
}
