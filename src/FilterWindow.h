/*
 * FilterWindow.h
 *
 *  Created on: Aug 12, 2013
 *      Author: matej
 */

#ifndef FILTERWINDOW_H_
#define FILTERWINDOW_H_

void filter_data_received(int packetId, DictionaryIterator* data);
void init_filter_window();

#endif /* FILTERWINDOW_H_ */
