/*
 * Dialer2.h
 *
 *  Created on: Aug 13, 2013
 *      Author: matej
 */

#ifndef PEBBLEDIALER_H_
#define PEBBLEDIALER_H_

#ifdef PBL_SDK_3
	#define STATUSBAR_Y_OFFSET STATUS_BAR_LAYER_HEIGHT
	#define ACTIONBAR_X_OFFSET 0
#else
	#define STATUSBAR_Y_OFFSET 0
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

extern bool closingMode;

void setCurWindow(int window);
void switchWindow(int window);
void closeApp(void);

#endif /* PEBBLEDIALER_H_ */
