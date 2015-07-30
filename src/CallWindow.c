#include <pebble.h>
#include "pebble_fonts.h"
#include "pebble.h"
#include "util.h"
#include "PebbleDialer.h"
#include "MainMenuWindow.h"
#include "StrokedTextLayer.h"
#include "ActionsMenu.h"

static Window* window;

static StrokedTextLayer* title;
static char timerText[10];

static StrokedTextLayer* callerName;
static StrokedTextLayer* callerNumType;
static StrokedTextLayer* callerNumber;

static GBitmap* buttonMicOn;
static GBitmap* buttonMicOff;
static GBitmap* buttonAnswer;
static GBitmap* buttonEndCall;
static GBitmap* buttonSpeakerOn;
static GBitmap* buttonSpeakerOff;

#ifdef PBL_COLOR
static uint16_t callerImageSize;
static uint8_t* bitmapReceivingBuffer = NULL;
static uint16_t bitmapReceivingBufferHead;
static GBitmap* callerBitmap = NULL;
static BitmapLayer* callerBitmapLayer;
#endif

static GBitmap** indexedIcons[6] = {&buttonAnswer, &buttonEndCall, &buttonMicOn, &buttonMicOff, &buttonSpeakerOn, &buttonSpeakerOff };

static ActionBarLayer* actionBar;

#ifdef PBL_SDK_3
	static StatusBarLayer* statusBar;
#endif

static bool callEstablished;
static uint16_t elapsedTime = 0;
static bool nameExist;
static bool vibrate;

static bool vibratingNow = false;

static char callerNameText[101];
static char callerNumTypeText[31];
static char callerNumberText[31];

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
	else if (number < 100)
	{
		string[offset] = iConvert[0];
		string[offset + 1] = iConvert[1];
	}
	else
	{
		string[offset] = iConvert[0];
		string[offset + 1] = iConvert[1];
		string[offset + 2] = iConvert[2];
	}
}

static void updateTimer(void)
{
	uint16_t minutes = elapsedTime / 60;
	uint16_t seconds = elapsedTime % 60;

	uint8_t extraOffset = minutes > 99 ? 1 : 0;

	convertTwoNumber(minutes, timerText, 0);
	timerText[2 + extraOffset] = ':';
	convertTwoNumber(seconds, timerText, 3 + extraOffset);

	stroked_text_layer_set_text(title, timerText);

}

static void updateTextFields(void)
{
	stroked_text_layer_set_text(callerNumber, callerNumberText);

	if (nameExist)
	{
		stroked_text_layer_set_text(callerName, callerNameText);
		stroked_text_layer_set_text(callerNumType, callerNumTypeText);

		layer_set_hidden(stroked_text_layer_get_layer(callerNumType), false);
		layer_set_hidden(stroked_text_layer_get_layer(callerNumber), false);

	}
	else
	{
		stroked_text_layer_set_text(callerName, callerNumberText);

		layer_set_hidden(stroked_text_layer_get_layer(callerNumType), true);
		layer_set_hidden(stroked_text_layer_get_layer(callerNumber), true);
	}

	if (callEstablished)
	{
		updateTimer();
	}
	else
	{
		stroked_text_layer_set_text(title, "Incoming Call");
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

static void sendMenuPick(uint8_t buttonId)
{
	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);

	dict_write_uint8(iterator, 0, 5);
	dict_write_uint8(iterator, 1, 0);
	dict_write_uint8(iterator, 2, buttonId);

	app_message_outbox_send();
}

static void button_up_press(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		actions_menu_move_up();
		return;
	}

	sendAction(0);
}

static void button_select_press(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		sendMenuPick(actions_menu_get_selected_index());
		actions_menu_hide();
		return;
	}

	sendAction(1);
}

static void button_down_press(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		actions_menu_move_down();
		return;
	}

	sendAction(2);
}

static void button_up_hold(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		actions_menu_move_up();
		return;
	}

	sendAction(3);
}

static void button_select_hold(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		return;
	}

	sendAction(4);
}

static void button_down_hold(ClickRecognizerRef recognizer, Window *window)
{
	if (actions_menu_is_displayed())
	{
		actions_menu_move_down();
		return;
	}

	sendAction(5);
}

static void button_back_press(ClickRecognizerRef recognizer, void* context)
{
	if (actions_menu_is_displayed())
	{
		actions_menu_hide();
	}
	else
		window_stack_pop(true);
}


static void shake(AccelAxisType axis, int32_t direction)
{
	if (vibratingNow) //Vibration seems to generate a lot of false positives
		return;

	sendAction(6);
}

static void config_provider_callscreen(void* context) {
	window_single_click_subscribe(BUTTON_ID_UP, (ClickHandler) button_up_press);
	window_single_click_subscribe(BUTTON_ID_DOWN, (ClickHandler) button_down_press);
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler) button_select_press);
	window_long_click_subscribe(BUTTON_ID_UP, 700, (ClickHandler) button_up_hold, NULL);
	window_long_click_subscribe(BUTTON_ID_SELECT, 700, (ClickHandler) button_select_hold, NULL);
	window_long_click_subscribe(BUTTON_ID_DOWN, 700, (ClickHandler) button_down_hold, NULL);
	window_single_click_subscribe(BUTTON_ID_BACK, (ClickHandler) button_back_press);

}

void call_window_data_received(uint8_t module, uint8_t packet, DictionaryIterator *received) {
	if (module == 1)
	{
		if (packet == 0)
		{
			uint8_t* flags = dict_find(received, 4)->value->data;
			callEstablished = flags[0] == 1;
			nameExist = flags[1] == 1;
			vibrate = flags[5] == 1 && canVibrate();

			uint8_t topIcon = flags[2];
			uint8_t middleIcon = flags[3];
			uint8_t bottomIcon = flags[4];

			action_bar_layer_set_icon(actionBar, BUTTON_ID_UP, *indexedIcons[topIcon]);
			action_bar_layer_set_icon(actionBar, BUTTON_ID_SELECT, *indexedIcons[middleIcon]);
			action_bar_layer_set_icon(actionBar, BUTTON_ID_DOWN, *indexedIcons[bottomIcon]);

			strcpy(callerNumberText, dict_find(received, 3)->value->cstring);
			if (nameExist)
			{
				strcpy(callerNumTypeText, dict_find(received, 2)->value->cstring);
			}

			if (callEstablished)
				elapsedTime = dict_find(received, 5)->value->uint16;

			updateTextFields();

#ifdef PBL_COLOR
			Tuple* callerImageSizeTuple = dict_find(received, 7);

			if (callerImageSizeTuple != NULL)
			{
				callerImageSize = callerImageSizeTuple->value->uint16;
				if (bitmapReceivingBuffer != NULL)
					free(bitmapReceivingBuffer);
				bitmapReceivingBuffer = malloc(callerImageSize);
				bitmapReceivingBufferHead = 0;
			}
#endif
		}
		else if (packet == 1)
		{
			if (!nameExist)
				return;

			strcpy(callerNameText, dict_find(received, 2)->value->cstring);
			stroked_text_layer_set_text(callerName, callerNameText);
		}
#ifdef PBL_COLOR
		else if (packet == 2)
		{
			if (bitmapReceivingBuffer == NULL)
				return;

			uint8_t* buffer = dict_find(received, 2)->value->data;
			uint16_t bufferSize = callerImageSize - bitmapReceivingBufferHead;
			bool finished = true;
			if (bufferSize > 116)
			{
				finished = false;
				bufferSize = 116;
			}

			memcpy(&bitmapReceivingBuffer[bitmapReceivingBufferHead], buffer, bufferSize);
			bitmapReceivingBufferHead += bufferSize;


			if (finished)
			{
				callerBitmap = gbitmap_create_from_png_data(bitmapReceivingBuffer, callerImageSize);
				bitmap_layer_set_bitmap(callerBitmapLayer, callerBitmap);
				free(bitmapReceivingBuffer);

				bitmapReceivingBuffer = NULL;
			}
		}
#endif
	}
}

static void vibration_stopped(void* data)
{
	vibratingNow = false;
}


static void second_tick()
{
	if (callEstablished)
	{
		elapsedTime++;
		updateTimer();
	}
	else if (vibrate)
	{
		vibratingNow = true;
		vibes_double_pulse();
		app_timer_register(500, vibration_stopped, NULL);
	}
}

static void window_show(Window* me)
{
	if (config_lightCallWindow)
		light_enable_interaction();
}

static void window_load(Window* me)
{
	Layer* topLayer = window_get_root_layer(window);

	#ifdef  PBL_COLOR
		callerBitmapLayer = bitmap_layer_create(GRect(0,STATUSBAR_Y_OFFSET, 144 - ACTION_BAR_WIDTH, 152));
		bitmap_layer_set_alignment(callerBitmapLayer, GAlignCenter);
		layer_add_child(topLayer, bitmap_layer_get_layer(callerBitmapLayer));
	#endif

	title = stroked_text_layer_create(GRect(5, STATUSBAR_Y_OFFSET,144 - ACTION_BAR_WIDTH - 5,30));
	stroked_text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, stroked_text_layer_get_layer(title));

	callerName = stroked_text_layer_create(GRect(5, 30 + STATUSBAR_Y_OFFSET, 144 - ACTION_BAR_WIDTH - 5, 90));
	stroked_text_layer_set_font(callerName, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(topLayer, stroked_text_layer_get_layer(callerName));

	callerNumType = stroked_text_layer_create(GRect(5,100 + STATUSBAR_Y_OFFSET,144 - ACTION_BAR_WIDTH - 5,20));
	layer_add_child(topLayer, stroked_text_layer_get_layer(callerNumType));

	callerNumber = stroked_text_layer_create(GRect(5,122 + STATUSBAR_Y_OFFSET,144 - ACTION_BAR_WIDTH - 5,30));
	layer_add_child(topLayer, stroked_text_layer_get_layer(callerNumber));

	buttonAnswer = gbitmap_create_with_resource(RESOURCE_ID_ANSWER);
	buttonEndCall = gbitmap_create_with_resource(RESOURCE_ID_ENDCALL);
	buttonMicOff = gbitmap_create_with_resource(RESOURCE_ID_MIC_OFF);
	buttonMicOn = gbitmap_create_with_resource(RESOURCE_ID_MIC_ON);
	buttonSpeakerOn = gbitmap_create_with_resource(RESOURCE_ID_SPEAKER_ON);
	buttonSpeakerOff = gbitmap_create_with_resource(RESOURCE_ID_SPEAKER_OFF);

	actionBar = action_bar_layer_create();
	action_bar_layer_set_click_config_provider(actionBar, (ClickConfigProvider) config_provider_callscreen);
	action_bar_layer_add_to_window(actionBar, window);

	layer_add_child(topLayer, action_bar_layer_get_layer(actionBar));

	#ifdef PBL_SDK_3
		statusBar = status_bar_layer_create();
		layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));
	#endif

	actions_menu_init();
	actions_menu_attach(topLayer);

	tick_timer_service_subscribe(SECOND_UNIT, (TickHandler) second_tick);
	accel_tap_service_subscribe(shake);
}

static void window_unload(Window* me)
{
	gbitmap_destroy(buttonAnswer);
	gbitmap_destroy(buttonEndCall);
	gbitmap_destroy(buttonMicOff);
	gbitmap_destroy(buttonMicOn);
	gbitmap_destroy(buttonSpeakerOn);
	gbitmap_destroy(buttonSpeakerOff);

	stroked_text_layer_destroy(title);
	stroked_text_layer_destroy(callerName);
	stroked_text_layer_destroy(callerNumType);
	stroked_text_layer_destroy(callerNumber);
	action_bar_layer_destroy(actionBar);

	#ifdef PBL_SDK_3
		status_bar_layer_destroy(statusBar);
	#endif


	#ifdef PBL_COLOR
		if (bitmapReceivingBuffer != NULL)
		{
			free(bitmapReceivingBuffer);
			bitmapReceivingBuffer = NULL;
		}

		if (callerBitmap != NULL)
		{
			gbitmap_destroy(callerBitmap);
			callerBitmap = NULL;
		}

		bitmap_layer_destroy(callerBitmapLayer);
	#endif

	actions_menu_deinit();

	window_destroy(me);

	tick_timer_service_unsubscribe();
	accel_tap_service_unsubscribe();
}

void call_window_init(void)
{
	elapsedTime = 0;
	callEstablished = false;
	nameExist = true;

	window = window_create();

	window_set_window_handlers(window, (WindowHandlers) {
		.load = (WindowHandler) window_load,
		.unload = (WindowHandler) window_unload,
		.appear = window_show

	});

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
