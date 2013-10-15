#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "DialerMenu.h"
#include "FilterWindow.h"
#include "ContactsWindow.h"
#include "callscreen.h"
#include "NumberPicker.h"
#include "CallLog.h"

uint8_t curWindow = 0;

#define MY_UUID { 0x15, 0x8A, 0x07, 0x4D, 0x85, 0xCE, 0x43, 0xD2, 0xAB, 0x7D, 0x14, 0x41, 0x6D, 0xDC, 0x10, 0x58 }

PBL_APP_INFO(MY_UUID,
		"Pebble Dialer", "matejdro",
		2, 0, /* App version */
		RESOURCE_ID_ICON,
		APP_INFO_STANDARD_APP);

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

void handle_init(AppContextRef ctx) {
	resource_init_current_app(&VERSION);

	//Show menu window that displays loading
	init_menu_window();
}

void second_tick(AppContextRef app_ctx, PebbleTickEvent *event)
{
	if (curWindow == 10)
	{
		callscreen_second();
	}
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
			.init_handler = &handle_init,
			.messaging_info =
			{
					.buffer_sizes =
					{
							.inbound = 124,
							.outbound = 50
					},
					.default_callbacks.callbacks = {
							.in_received = received_data,
							.out_sent = data_delivered,
					},
			},
			.tick_info = {
					.tick_handler = &second_tick,
					.tick_units = SECOND_UNIT
			}
	};
	app_event_loop(params, &handlers);
}
