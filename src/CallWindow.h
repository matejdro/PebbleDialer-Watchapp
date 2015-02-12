/*
 * callscreen.h
 *
 *  Created on: May 31, 2013
 *      Author: matej
 */

#ifndef CALLWINDOW_H_
#define CALLWINDOW_H_

void call_window_init();
void call_window_data_received(uint8_t id, DictionaryIterator *received);

#endif /* CALLWINDOW_H_ */
