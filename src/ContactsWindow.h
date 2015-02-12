/*
 * ContactsWindow.h
 *
 *  Created on: Aug 13, 2013
 *      Author: matej
 */

#ifndef CONTACTSWINDOW_H_
#define CONTACTSWINDOW_H_

void contacts_window_data_received(int packetId, DictionaryIterator* data);
void contacts_window_data_delivered(void);
void contacts_window_init(void);
void contacts_window_stop_filtering(void);

#endif /* CONTACTSWINDOW_H_ */
