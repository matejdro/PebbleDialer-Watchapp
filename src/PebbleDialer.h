/*
 * Dialer2.h
 *
 *  Created on: Aug 13, 2013
 *      Author: matej
 */

#ifndef PEBBLEDIALER_H_
#define PEBBLEDIALER_H_


#define SCREEN_WIDTH  (PBL_IF_RECT_ELSE(144, 180))
#define SCREEN_HEIGHT (PBL_IF_RECT_ELSE(168, 180))

#define HEIGHT_BELOW_STATUSBAR (SCREEN_HEIGHT - STATUS_BAR_LAYER_HEIGHT)

#ifdef PBL_PLATFORM_APLITE
    #define PBL_LOW_MEMORY
#endif

extern bool config_dontClose;
extern uint8_t config_numOfGroups;
extern bool config_noMenu;
extern bool config_noFilterGroups;
extern bool config_lightCallWindow;
extern bool config_dontVibrateWhenCharging;
extern bool config_enableCallTimer;
extern bool config_enableOutgoingCallPopup;
extern uint8_t config_fontTimer;
extern uint8_t config_fontName;
extern uint8_t config_fontNumberType;
extern uint8_t config_fontNumber;

const char* config_getFontResource(int id);

extern bool closingMode;
extern bool exitOnDataDelivered;


void setCurWindow(int window);
void switchWindow(int window);
void closeApp(void);
bool canVibrate(void);
void onOutgoingCallEstablished();

#endif /* PEBBLEDIALER_H_ */
