/*
 * Dialer2.h
 *
 *  Created on: Aug 13, 2013
 *      Author: matej
 */

#ifndef PEBBLEDIALER_H_
#define PEBBLEDIALER_H_

extern bool config_dontClose;
extern uint8_t config_numOfGroups;
extern bool config_noMenu;
extern bool config_noFilterGroups;

extern bool closingMode;

void setCurWindow(int window);
void switchWindow(int window);
void closeApp(void);

#endif /* PEBBLEDIALER_H_ */
