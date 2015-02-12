#include "pebble_fonts.h"
#include "pebble.h"
#include "util.h"
#include "PebbleDialer.h"
#include "MainMenuWindow.h"

static Window* window;

static TextLayer* title;
static char timerText[6];

static TextLayer* callerName;
static TextLayer* callerNumType;
static TextLayer* callerNumber;

static GBitmap* buttonMicOn;
static GBitmap* buttonMicOff;
static GBitmap* buttonAnswer;
static GBitmap* buttonEndCall;
static GBitmap* buttonSpeakerOn;
static GBitmap* buttonSpeakerOff;

static ActionBarLayer* actionBar;

static bool callEstablished;
static uint16_t elapsedTime = 0;
static bool speakerOn;
static bool micOn;
static bool nameExist;

static char callerNameText[21];
static char callerNumTypeText[21];
static char callerNumberText[21];

static void convertTwoNumber(int number, char* string, int offset)
{
	if (number == 0)
	{
		string[offset] = '0';
		string[offset + 1] = '0';
		return;
	}

	char* iConvert = itoa(number);

	if (number < 10)
	{
		string[offset] = '0';
		string[offset + 1] = iConvert[0];
	}
	else
	{
		string[offset] = iConvert[0];
		string[offset + 1] = iConvert[1];
	}
}

static void updateTimer(void)
{
	int minutes = elapsedTime / 60;
	int seconds = elapsedTime % 60;
	if (minutes > 99)
		minutes = 99;

	convertTwoNumber(minutes, timerText, 0);
	timerText[2] = ':';
	convertTwoNumber(seconds, timerText, 3);

	text_layer_set_text(title, timerText);

}

static void updateTextFields(void)
{
	if (nameExist)
	{
		text_layer_set_text(callerName, callerNameText);
		text_layer_set_text(callerNumType, callerNumTypeText);
		text_layer_set_text(callerNumber, callerNumberText);

		layer_set_hidden((Layer * ) callerNumType, false);
		layer_set_hidden((Layer * ) callerNumber, false);
	}
	else
	{
		text_layer_set_text(callerName, callerNumberText);

		layer_set_hidden((Layer * ) callerNumType, true);
		layer_set_hidden((Layer * ) callerNumber, true);
	}

	if (callEstablished)
	{
		updateTimer();
	}
	else
	{
		text_layer_set_text(title, "Incoming Call");
	}
}

static void updateActionBar(void)
{
	if (callEstablished)
	{
		action_bar_layer_set_icon(actionBar, BUTTON_ID_UP, micOn ? buttonMicOn : buttonMicOff);
		action_bar_layer_set_icon(actionBar, BUTTON_ID_SELECT, speakerOn ? buttonSpeakerOn : buttonSpeakerOff);
		action_bar_layer_set_icon(actionBar, BUTTON_ID_DOWN, buttonEndCall);
	}
	else
	{
		action_bar_layer_set_icon(actionBar, BUTTON_ID_UP, speakerOn ? buttonSpeakerOn : buttonSpeakerOff);
		action_bar_layer_set_icon(actionBar, BUTTON_ID_SELECT, buttonAnswer);
		action_bar_layer_set_icon(actionBar, BUTTON_ID_DOWN, buttonEndCall);
	}
}

static void sendAction(int buttonId)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);

	dict_write_uint8(iterator, 0, 1);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint8(iterator, 2, buttonId);

	app_message_outbox_send();
}

static void button_up_press(ClickRecognizerRef recognizer, Window *window)
{
	if (callEstablished)
	{
		micOn = !micOn;
	}
	else
	{
		speakerOn = !speakerOn;
	}

	updateActionBar();
	sendAction(0);
}

static void button_select_press(ClickRecognizerRef recognizer, Window *window)
{
	if (callEstablished)
	{
		speakerOn = !speakerOn;
		updateActionBar();
	}

	sendAction(1);
}

static void button_down_press(ClickRecognizerRef recognizer, Window *window)
{
	sendAction(2);
}

static void config_provider_callscreen(void* context) {
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) button_up_press);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) button_down_press);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) button_select_press);
}

void call_window_data_received(uint8_t id, DictionaryIterator *received) {
	if (id == 0)
	{
		uint8_t* flags = dict_find(received, 5)->value->data;
		callEstablished = flags[0] == 1;
		speakerOn = flags[1] == 1;
		micOn = flags[2] == 1;
		nameExist = flags[3] == 1;

		strcpy(callerNumberText, dict_find(received, 4)->value->cstring);
		if (nameExist)
		{
			strcpy(callerNameText, dict_find(received, 2)->value->cstring);
			strcpy(callerNumTypeText, dict_find(received, 3)->value->cstring);
		}

		if (callEstablished)
			elapsedTime = dict_find(received, 6)->value->uint16;

		updateActionBar();
		updateTextFields();
	}
}

static void second_tick()
{
	if (callEstablished)
	{
		elapsedTime++;
		updateTimer();
	}
	else
	{
		if (speakerOn) vibes_double_pulse();
	}
}

static void window_load(Window* me)
{
	Layer* topLayer = window_get_root_layer(window);

	title = text_layer_create(GRect(5,0,144 - 30,30));
	text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(title, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)title);

	callerName = text_layer_create(GRect(5,40,144 - 30,60));
	text_layer_set_font(callerName, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(callerName, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)callerName);

	callerNumType = text_layer_create(GRect(5,100,144 - 30,20));
	text_layer_set_font(callerNumType, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(callerNumType, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)callerNumType);

	callerNumber = text_layer_create(GRect(5,122,144 - 30,30));
	text_layer_set_font(callerNumber, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(callerNumber, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)callerNumber);

	buttonAnswer = gbitmap_create_with_resource(RESOURCE_ID_ANSWER);
	buttonEndCall = gbitmap_create_with_resource(RESOURCE_ID_ENDCALL);
	buttonMicOff = gbitmap_create_with_resource(RESOURCE_ID_MIC_OFF);
	buttonMicOn = gbitmap_create_with_resource(RESOURCE_ID_MIC_ON);
	buttonSpeakerOn = gbitmap_create_with_resource(RESOURCE_ID_SPEAKER_ON);
	buttonSpeakerOff = gbitmap_create_with_resource(RESOURCE_ID_SPEAKER_OFF);

	actionBar = action_bar_layer_create();
	action_bar_layer_set_click_config_provider(actionBar, (ClickConfigProvider) config_provider_callscreen);
	action_bar_layer_add_to_window(actionBar, window);
}

static void window_unload(Window* me)
{
	gbitmap_destroy(buttonAnswer);
	gbitmap_destroy(buttonEndCall);
	gbitmap_destroy(buttonMicOff);
	gbitmap_destroy(buttonMicOn);
	gbitmap_destroy(buttonSpeakerOn);
	gbitmap_destroy(buttonSpeakerOff);

	text_layer_destroy(title);
	text_layer_destroy(callerName);
	text_layer_destroy(callerNumType);
	text_layer_destroy(callerNumber);
	action_bar_layer_destroy(actionBar);

	window_destroy(me);

	tick_timer_service_unsubscribe();
}

void call_window_init(void)
{
	elapsedTime = 0;
	callEstablished = false;
	speakerOn = true;
	micOn = true;
	nameExist = true;

	window = window_create();

	window_set_window_handlers(window, (WindowHandlers) {
		.load = (WindowHandler) window_load,
		.unload = (WindowHandler) window_unload

	});

	tick_timer_service_subscribe(SECOND_UNIT, (TickHandler) second_tick);

	window_stack_push(window, false);
	setCurWindow(1);

	if (config_noMenu)
	{
		if (!config_dontClose)
			main_menu_close();
		else
			main_menu_show_closing();
	}
}
