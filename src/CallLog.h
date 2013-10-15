/*
 * CallLog.h
 *
 *  Created on: Aug 20, 2013
 *      Author: matej
 */

#ifndef CALLLOG_H_
#define CALLLOG_H_

void init_call_log_window();
void cl_data_received(int packetId, DictionaryIterator* data);

#endif /* CALLLOG_H_ */
