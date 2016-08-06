#include "pebble.h"
#include "pebble_fonts.h"

#include "CallLogWindow.h"
#include "ContactsWindow.h"
#include "CallWindow.h"
#include "MainMenuWindow.h"
#include "NumberPickerWindow.h"
#include "ActionsMenu.h"

const uint16_t PROTOCOL_VERSION = 10;

static uint8_t curWindow = 0;
static bool gotConfig = false;

bool config_dontClose;
bool config_noMenu;
uint8_t config_numOfGroups;
bool config_noFilterGroups;
bool config_lightCallWindow;
bool config_dontVibrateWhenCharging;
bool config_enableCallTimer;
bool config_enableOutgoingCallPopup;
uint8_t config_fontTimer;
uint8_t config_fontName;
uint8_t config_fontNumberType;
uint8_t config_fontNumber;

bool closingMode;
bool exitOnDataDelivered;

static const char* fonts[] = {
		FONT_KEY_GOTHIC_14,
		FONT_KEY_GOTHIC_14_BOLD,
		FONT_KEY_GOTHIC_18,
		FONT_KEY_GOTHIC_18_BOLD,
		FONT_KEY_GOTHIC_24,
		FONT_KEY_GOTHIC_24_BOLD,
		FONT_KEY_GOTHIC_28,
		FONT_KEY_GOTHIC_28_BOLD,
		FONT_KEY_BITHAM_30_BLACK,
		FONT_KEY_BITHAM_42_BOLD,
		FONT_KEY_BITHAM_42_LIGHT,
		FONT_KEY_BITHAM_42_MEDIUM_NUMBERS,
		FONT_KEY_BITHAM_34_MEDIUM_NUMBERS,
		FONT_KEY_BITHAM_34_LIGHT_SUBSET,
		FONT_KEY_BITHAM_18_LIGHT_SUBSET,
		FONT_KEY_ROBOTO_CONDENSED_21,
		FONT_KEY_ROBOTO_BOLD_SUBSET_49,
		FONT_KEY_DROID_SERIF_28_BOLD
};

const char* config_getFontResource(int id)
{
	return fonts[id];
}

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
	config_lightCallWindow = (data[2] & 0x08) != 0;
	config_dontVibrateWhenCharging = (data[2] & 0x10) != 0;
	config_enableCallTimer = (data[2] & 0x20) != 0;
	config_enableOutgoingCallPopup = (data[2] & 0x40) != 0;

	config_numOfGroups = data[3];
	config_fontTimer = data[4];
	config_fontName = data[5];
	config_fontNumberType = data[6];
	config_fontNumber = data[7];

	gotConfig = true;

	if (!config_noMenu)
		main_menu_show_menu();

}

bool canVibrate(void)
{
	return !config_dontVibrateWhenCharging || !battery_state_service_peek().is_plugged;
}

void onOutgoingCallEstablished()
{
	if (!config_enableOutgoingCallPopup)
		exitOnDataDelivered = true;
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

		call_window_data_received(destModule, packetId, received);
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
	else if (destModule == 5)
	{
		actions_menu_got_data(packetId, received);
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

	if (exitOnDataDelivered)
		window_stack_pop_all(true);
}

static uint32_t getCapabilities(uint16_t maxInboxSize)
{
	uint32_t serializedCapabilities = 0;

	serializedCapabilities |= PBL_IF_MICROPHONE_ELSE(0x01, 0x00);
	serializedCapabilities |= PBL_IF_COLOR_ELSE(0x02, 0x00);
	serializedCapabilities |= PBL_IF_ROUND_ELSE(0x04, 0x00);
	serializedCapabilities |= PBL_IF_SMARTSTRAP_ELSE(0x08, 0x00);
	serializedCapabilities |= PBL_IF_HEALTH_ELSE(0x10, 0x00);
	serializedCapabilities |= maxInboxSize << 16;

	return serializedCapabilities;
}

int main() {
	app_message_register_outbox_sent(data_delivered);
	app_message_register_inbox_received(received_data);

	uint16_t appmessage_max_size = app_message_inbox_size_maximum();
	if (appmessage_max_size > 4096)
		appmessage_max_size = 4096; //Limit inbox size to conserve RAM.

	#ifdef PBL_PLATFORM_APLITE
		//Aplite has so little memory, we can't squeeze much more than that out of appmessage buffer.
		appmessage_max_size = 124;
	#endif

	app_message_open(appmessage_max_size, 124);

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 0);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint16(iterator, 2, PROTOCOL_VERSION);
	dict_write_uint32(iterator, 3, getCapabilities(appmessage_max_size));

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
	app_message_outbox_send();

	switchWindow(0);
	app_event_loop();
	window_stack_pop_all(false);
	return 0;
}
