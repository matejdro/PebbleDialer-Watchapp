#include "pebble_fonts.h"
#include "pebble_os.h"
#include "pebble_app.h"
#include "util.h"

Window callscreen;

TextLayer title;
char timerText[6];

TextLayer callerName;
TextLayer callerNumType;
TextLayer callerNumber;

HeapBitmap buttonMicOn;
HeapBitmap buttonMicOff;
HeapBitmap buttonAnswer;
HeapBitmap buttonEndCall;
HeapBitmap buttonSpeakerOn;
HeapBitmap buttonSpeakerOff;

ActionBarLayer actionBar;

bool callEstablished;
uint16_t elapsedTime = 0;
bool speakerOn;
bool micOn;
bool nameExist;

bool busy;

char callerNameText[21];
char callerNumTypeText[21];
char callerNumberText[21];

void convertTwoNumber(int number, char* string, int offset)
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

void updateTimer()
{
	int minutes = elapsedTime / 60;
	int seconds = elapsedTime % 60;
	if (minutes > 99)
		minutes = 99;

	convertTwoNumber(minutes, timerText, 0);
	timerText[2] = ':';
	convertTwoNumber(seconds, timerText, 3);

	text_layer_set_text(&title, timerText);

}

void renderTextFields()
{
	if (nameExist)
	{
		text_layer_set_text(&callerName, callerNameText);
		text_layer_set_text(&callerNumType, callerNumTypeText);
		text_layer_set_text(&callerNumber, callerNumberText);

		layer_set_hidden((Layer * ) &callerNumType, false);
		layer_set_hidden((Layer * ) &callerNumber, false);
	}
	else
	{
		text_layer_set_text(&callerName, callerNumberText);

		layer_set_hidden((Layer * ) &callerNumType, true);
		layer_set_hidden((Layer * ) &callerNumber, true);
	}

	if (callEstablished)
	{
		updateTimer();
	}
	else
	{
		text_layer_set_text(&title, "Incoming Call");
	}
}

void renderActionBar()
{
	if (callEstablished)
	{
		action_bar_layer_set_icon(&actionBar, BUTTON_ID_UP, micOn ? &buttonMicOn.bmp : &buttonMicOff.bmp);
		action_bar_layer_set_icon(&actionBar, BUTTON_ID_SELECT, speakerOn ? &buttonSpeakerOn.bmp : &buttonSpeakerOff.bmp);
		action_bar_layer_set_icon(&actionBar, BUTTON_ID_DOWN, &buttonEndCall.bmp);
	}
	else
	{
		action_bar_layer_set_icon(&actionBar, BUTTON_ID_UP, speakerOn ? &buttonSpeakerOn.bmp : &buttonSpeakerOff.bmp);
		action_bar_layer_set_icon(&actionBar, BUTTON_ID_SELECT, &buttonAnswer.bmp);
		action_bar_layer_set_icon(&actionBar, BUTTON_ID_DOWN, &buttonEndCall.bmp);
	}
}

void sendAction(int buttonId)
{
	DictionaryIterator *iterator;
	app_message_out_get(&iterator);

	dict_write_uint8(iterator, 0, 7);
	dict_write_uint8(iterator, 1, buttonId);

	app_message_out_send();
	app_message_out_release();

	busy = true;
}

void up_button_callscreen(ClickRecognizerRef recognizer, Window *window)
{
	if (busy) return;

	if (callEstablished)
	{
		micOn = !micOn;
	}
	else
	{
		speakerOn = !speakerOn;
	}

	renderActionBar();
	sendAction(0);
}

void center_button_callscreen(ClickRecognizerRef recognizer, Window *window)
{
	if (busy) return;

	if (callEstablished)
	{
		speakerOn = !speakerOn;
		renderActionBar();
	}

	sendAction(1);
}

void down_button_callscreen(ClickRecognizerRef recognizer, Window *window)
{
	if (busy) return;

	sendAction(2);
}

void config_provider_callscreen(ClickConfig **config, Window *window) {
	config[BUTTON_ID_UP]->click.handler=(ClickHandler) up_button_callscreen;

	config[BUTTON_ID_DOWN]->click.handler=(ClickHandler) down_button_callscreen;

	config[BUTTON_ID_SELECT]->click.handler=(ClickHandler) center_button_callscreen;
}

void callscreen_received_data(uint8_t id, DictionaryIterator *received) {
	if (id == 5)
	{
		uint8_t* flags = dict_find(received, 4)->value->data;
		callEstablished = flags[0] == 1;
		speakerOn = flags[1] == 1;
		micOn = flags[2] == 1;
		nameExist = flags[3] == 1;

		strcpy(callerNumberText, dict_find(received, 3)->value->cstring);
		if (nameExist)
		{
			strcpy(callerNameText, dict_find(received, 1)->value->cstring);
			strcpy(callerNumTypeText, dict_find(received, 2)->value->cstring);
		}

		if (callEstablished)
			elapsedTime = dict_find(received, 5)->value->uint16;

		renderActionBar();
		renderTextFields();
	}
}

void callscreen_data_delivered(DictionaryIterator *sent) {
	busy = false;
}

void callscreen_appears(Window *window)
{
	action_bar_layer_add_to_window(&actionBar, &callscreen);
}



void callscreen_second()
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

void callscreen_init()
{
	elapsedTime = 0;
	callEstablished = false;
	speakerOn = true;
	micOn = true;
	nameExist = true;

	busy = false;

	window_init(&callscreen, "Call");

	window_set_window_handlers(&callscreen, (WindowHandlers) {
		.appear = (WindowHandler)callscreen_appears,
	});

	window_stack_push(&callscreen, false);

	Layer* topLayer = window_get_root_layer(&callscreen);

	text_layer_init(&title, GRect(5,0,144 - 30,30));
	text_layer_set_font(&title, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(&title, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)&title);

	text_layer_init(&callerName, GRect(5,40,144 - 30,60));
	text_layer_set_font(&callerName, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(&callerName, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)&callerName);

	text_layer_init(&callerNumType, GRect(5,100,144 - 30,20));
	text_layer_set_font(&callerNumType, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(&callerNumType, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)&callerNumType);

	text_layer_init(&callerNumber, GRect(5,122,144 - 30,30));
	text_layer_set_font(&callerNumber, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(&callerNumber, GTextAlignmentCenter);
	layer_add_child(topLayer, (Layer *)&callerNumber);

	resource_init_current_app(&VERSION);
	heap_bitmap_init(&buttonAnswer, RESOURCE_ID_ANSWER);
	heap_bitmap_init(&buttonEndCall, RESOURCE_ID_ENDCALL);
	heap_bitmap_init(&buttonMicOff, RESOURCE_ID_MIC_OFF);
	heap_bitmap_init(&buttonMicOn, RESOURCE_ID_MIC_ON);
	heap_bitmap_init(&buttonSpeakerOn, RESOURCE_ID_SPEAKER_ON);
	heap_bitmap_init(&buttonSpeakerOff, RESOURCE_ID_SPEAKER_OFF);

	action_bar_layer_init(&actionBar);
	action_bar_layer_set_click_config_provider(&actionBar, (ClickConfigProvider) &config_provider_callscreen);

	renderActionBar();
	renderTextFields();
}
