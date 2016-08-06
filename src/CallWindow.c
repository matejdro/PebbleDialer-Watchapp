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

static GSize windowFrame;
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

static StatusBarLayer* statusBar;

static bool callEstablished;
static uint16_t elapsedTime = 0;
static bool vibrate;

static bool vibratingNow = false;

static char callerNameText[101];
static char callerNumTypeText[31];
static char callerNumberText[31];
static bool nameAtBottom;

static void vibration_cycle_finished(void *data);

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
	if (!callEstablished)
	{
		stroked_text_layer_set_text(title, "Incoming Call");
		return;
	}

	if (!config_enableCallTimer)
	{
		stroked_text_layer_set_text(title, "");
		return;
	}


	uint16_t minutes = elapsedTime / 60;
	uint16_t seconds = elapsedTime % 60;

	uint8_t extraOffset = minutes > 99 ? 1 : 0;

	convertTwoNumber(minutes, timerText, 0);
	timerText[2 + extraOffset] = ':';
	convertTwoNumber(seconds, timerText, 3 + extraOffset);

	stroked_text_layer_set_text(title, timerText);
}

static GRect moveAndCalculateTextSize(StrokedTextLayer* textLayer, int16_t yPosition, bool centerY, bool moveUp)
{
	layer_set_frame(stroked_text_layer_get_layer(textLayer), GRect(0, yPosition, windowFrame.w, 1000));
	GSize size = stroked_text_layer_get_content_size(textLayer);
	size.h += 3;

	if (centerY)
		yPosition -= size.h / 2;
	if (moveUp)
		yPosition -= size.h;

	GRect frame = GRect(0, yPosition, windowFrame.w, size.h);
	layer_set_frame(stroked_text_layer_get_layer(textLayer), frame);
	return frame;
	return GRect(0, 0, 0, 0);
}

static void updateTextFields(void)
{
	//Timer at the top
	updateTimer();
	int16_t timerY = STATUS_BAR_LAYER_HEIGHT;
	GRect timerPosition = moveAndCalculateTextSize(title, timerY, false, false);

	//Caller name is below timer
	stroked_text_layer_set_text(callerName, nameAtBottom ? "" : callerNameText);
	int16_t nameY = timerPosition.origin.y + timerPosition.size.h;
	moveAndCalculateTextSize(callerName, nameY, false, false);

	//Caller number is near the bottom
	stroked_text_layer_set_text(callerNumber, nameAtBottom ? callerNameText : callerNumberText);
	int16_t callerNumberY = windowFrame.h - 10;
	GRect callerNumberFrame = moveAndCalculateTextSize(callerNumber, callerNumberY, false, true);

	//Caller number type is above caller number
	stroked_text_layer_set_text(callerNumType, callerNumTypeText);
	int16_t callerNumberTypeY = callerNumberFrame.origin.y;
	moveAndCalculateTextSize(callerNumType, callerNumberTypeY, false, true);
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


static void vibrate_cycle(void *data)
{
	if (vibrate)
	{
		vibratingNow = true;
		vibes_double_pulse();
		app_timer_register(500, vibration_cycle_finished, NULL);
	}
}

static void vibration_cycle_finished(void *data)
{
	vibratingNow = false;
	app_timer_register(500, vibrate_cycle, NULL);
}


static void second_tick()
{
	if (callEstablished)
	{
		elapsedTime++;
		updateTimer();
	}
}


static void start_vibrating()
{
	vibrate = true;
	vibrate_cycle(NULL);
}

static void stop_vibrating()
{
	vibrate = false;
	vibes_cancel();
}

void call_window_data_received(uint8_t module, uint8_t packet, DictionaryIterator *received) {

	if (module == 1)
	{
		if (packet == 0)
		{
			uint8_t* flags = dict_find(received, 4)->value->data;
			callEstablished = flags[0] == 1;
			nameAtBottom = flags[1] == 1;
			bool phoneAllowsVibration = flags[5] == 1;

			if (!callEstablished && phoneAllowsVibration && canVibrate())
				start_vibrating();
			else
				stop_vibrating();

			uint8_t topIcon = flags[2];
			uint8_t middleIcon = flags[3];
			uint8_t bottomIcon = flags[4];

			action_bar_layer_set_icon(actionBar, BUTTON_ID_UP, *indexedIcons[topIcon]);
			action_bar_layer_set_icon(actionBar, BUTTON_ID_SELECT, *indexedIcons[middleIcon]);
			action_bar_layer_set_icon(actionBar, BUTTON_ID_DOWN, *indexedIcons[bottomIcon]);

			strcpy(callerNumberText, dict_find(received, 3)->value->cstring);
			strcpy(callerNumTypeText, dict_find(received, 2)->value->cstring);

			if (callEstablished)
				elapsedTime = dict_find(received, 5)->value->uint16;

			updateTextFields();

#ifdef PBL_COLOR
			Tuple* callerImageSizeTuple = dict_find(received, 7);

			if (callerImageSizeTuple != NULL)
			{
				callerImageSize = callerImageSizeTuple->value->uint16;
				if (callerBitmap != NULL)
				{
					bitmap_layer_set_bitmap(callerBitmapLayer, NULL);
					gbitmap_destroy(callerBitmap);
					callerBitmap = NULL;
				}
				if (bitmapReceivingBuffer != NULL)
				{
					free(bitmapReceivingBuffer);
				}

				bitmapReceivingBuffer = malloc(callerImageSize);
				bitmapReceivingBufferHead = 0;
			}
#endif
		}
		else if (packet == 1)
		{
			strcpy(callerNameText, dict_find(received, 2)->value->cstring);
			updateTextFields();
		}
#ifdef PBL_COLOR
		else if (packet == 2)
		{
			if (bitmapReceivingBuffer == NULL)
				return;

			uint16_t bufferSize = dict_find(received, 2)->value->uint16;
			uint8_t* buffer = dict_find(received, 3)->value->data;

			memcpy(&bitmapReceivingBuffer[bitmapReceivingBufferHead], buffer, bufferSize);
			bitmapReceivingBufferHead += bufferSize;
			bool finished = bitmapReceivingBufferHead == callerImageSize;

			if (finished)
			{
				callerBitmap = gbitmap_create_from_png_data(bitmapReceivingBuffer, callerImageSize);
				bitmap_layer_set_bitmap(callerBitmapLayer, callerBitmap);
			}
		}
#endif
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

	windowFrame = layer_get_frame(topLayer).size;
	windowFrame.w -= ACTION_BAR_WIDTH;

	#ifdef  PBL_COLOR
		callerBitmapLayer = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(0, 25),STATUS_BAR_LAYER_HEIGHT, SCREEN_WIDTH - ACTION_BAR_WIDTH - PBL_IF_RECT_ELSE(0, 25), HEIGHT_BELOW_STATUSBAR));
		bitmap_layer_set_alignment(callerBitmapLayer, GAlignCenter);
		layer_add_child(topLayer, bitmap_layer_get_layer(callerBitmapLayer));
	#endif


	title = stroked_text_layer_create(GRectZero);
	stroked_text_layer_set_font(title, fonts_get_system_font(config_getFontResource(config_fontTimer)));
	layer_add_child(topLayer, stroked_text_layer_get_layer(title));

	callerName = stroked_text_layer_create(GRectZero);
	stroked_text_layer_set_font(callerName, fonts_get_system_font(config_getFontResource(config_fontName)));
	stroked_text_layer_set_text_flow(callerName, true);
	layer_add_child(topLayer, stroked_text_layer_get_layer(callerName));

	callerNumType = stroked_text_layer_create(GRectZero);
	stroked_text_layer_set_font(callerNumType, fonts_get_system_font(config_getFontResource(config_fontNumberType)));
	stroked_text_layer_set_text_flow(callerNumType, true);
	layer_add_child(topLayer, stroked_text_layer_get_layer(callerNumType));

	callerNumber = stroked_text_layer_create(GRectZero);
	stroked_text_layer_set_font(callerNumber, fonts_get_system_font(config_getFontResource(config_fontNumber)));
	stroked_text_layer_set_text_flow(callerNumber, true);
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

	statusBar = status_bar_layer_create();
	layer_add_child(topLayer, status_bar_layer_get_layer(statusBar));

	actions_menu_init();
	actions_menu_attach(topLayer);

	if (config_enableCallTimer)
	{
		tick_timer_service_subscribe(SECOND_UNIT, (TickHandler) second_tick);
	}
	accel_tap_service_subscribe(shake);

	callerNameText[0] = 0;
	callerNumberText[0] = 0;
	callerNumTypeText[0] = 0;
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

	status_bar_layer_destroy(statusBar);


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

	window = window_create();

	window_set_window_handlers(window, (WindowHandlers) {
		.load = (WindowHandler) window_load,
		.unload = (WindowHandler) window_unload,
		.appear = window_show

	});

	window_stack_pop_all(false);
	if (!config_noMenu)
		main_menu_show_closing();

	window_stack_push(window, true);
	setCurWindow(1);
}
