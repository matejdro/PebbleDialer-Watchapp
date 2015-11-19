/*
 * Dialer2.h
 *
 *  Created on: Aug 13, 2013
 *      Author: matej
 */

#ifndef PEBBLEDIALER_H_
#define PEBBLEDIALER_H_


#ifdef PBL_SDK_3
	#define SCREEN_WIDTH  (PBL_IF_RECT_ELSE(144, 180))
	#define SCREEN_HEIGHT (PBL_IF_RECT_ELSE(168, 180))

	#define STATUSBAR_Y_OFFSET STATUS_BAR_LAYER_HEIGHT
	#define HEIGHT_BELOW_STATUSBAR (SCREEN_HEIGHT - STATUSBAR_Y_OFFSET)
	#define ACTIONBAR_X_OFFSET 0
#else
	#define PBL_IF_RECT_ELSE(if_true, if_false) (if_true)
	#define PBL_IF_ROUND_ELSE(if_true, if_false) (if_false)
	#define SCREEN_WIDTH  144
	#define SCREEN_HEIGHT 168

	#define STATUSBAR_Y_OFFSET 0
	#define HEIGHT_BELOW_STATUSBAR (SCREEN_HEIGHT - 16)
	#define ACTIONBAR_X_OFFSET 10
#endif

#define HALF_ACTIONBAR_X_OFFSET ACTIONBAR_X_OFFSET / 2


#ifdef PBL_COLOR
	#define PNG_COMPOSITING_MODE GCompOpSet
#else
	#define PNG_COMPOSITING_MODE GCompOpAssign
#endif

extern bool config_dontClose;
extern uint8_t config_numOfGroups;
extern bool config_noMenu;
extern bool config_noFilterGroups;
extern bool config_lightCallWindow;
extern bool config_dontVibrateWhenCharging;
extern uint8_t config_fontTimer;
extern uint8_t config_fontName;
extern uint8_t config_fontNumberType;
extern uint8_t config_fontNumber;

const char* config_getFontResource(int id);

extern bool closingMode;


void setCurWindow(int window);
void switchWindow(int window);
void closeApp(void);
bool canVibrate(void);

#endif /* PEBBLEDIALER_H_ */
