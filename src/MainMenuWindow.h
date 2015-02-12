/*
 * DialerMenu.h
 *
 *  Created on: Aug 12, 2013
 *      Author: matej
 */

#ifndef MAINMENUWINDOW_H_
#define MAINMENUWINDOW_H_

void main_menu_init(void);
void main_menu_close(void);
void main_menu_data_received(int packetId, DictionaryIterator* data);
void main_menu_show_menu(void);
void main_menu_show_closing(void);
void main_menu_show_old_watchapp(void);
void main_menu_show_old_android(void);

#endif /* MAINMENUWINDOW_H_ */
