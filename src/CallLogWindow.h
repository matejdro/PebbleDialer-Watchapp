/*
 * CallLog.h
 *
 *  Created on: Aug 20, 2013
 *      Author: matej
 */

#ifndef CALLLOGWINDOW_H_
#define CALLLOGWINDOW_H_

void call_log_window_init(void);
void call_log_window_data_received(int packetId, DictionaryIterator* data);
void call_log_window_data_sent(void);

#endif /* CALLLOGWINDOW_H_ */
