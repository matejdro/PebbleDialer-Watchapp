/*
 * NumberPicker.h
 *
 *  Created on: Aug 19, 2013
 *      Author: matej
 */

#ifndef NUMBERPICKERWINDOW_H_
#define NUMBERPICKERWINDOW_H_

void number_picker_window_data_received(int packetId, DictionaryIterator* data);
void number_picker_window_data_sent(void);

void number_picker_window_init(void);

#endif /* NUMBERPICKERWINDOW_H_ */
