/*
 * callscreen.h
 *
 *  Created on: May 31, 2013
 *      Author: matej
 */

#ifndef CALLSCREEN_H_
#define CALLSCREEN_H_

void callscreen_init();
void callscreen_received_data(uint8_t id, DictionaryIterator *received);
void callscreen_second();
void callscreen_data_delivered(DictionaryIterator *sent);

#endif /* CALLSCREEN_H_ */
