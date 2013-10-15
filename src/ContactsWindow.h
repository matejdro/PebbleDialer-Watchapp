/*
 * ContactsWindow.h
 *
 *  Created on: Aug 13, 2013
 *      Author: matej
 */

#ifndef CONTACTSWINDOW_H_
#define CONTACTSWINDOW_H_

void contacts_data_received(int packetId, DictionaryIterator* data);
void init_contacts_window(char* names);

#endif /* CONTACTSWINDOW_H_ */
