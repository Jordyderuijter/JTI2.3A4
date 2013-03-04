/* 
 * File:   menu.h
 * Author: Bas
 *
 * Created on 19 februari 2013, 12:45
 */

// Hours/minutes struct.
struct hm
{
    short hm_hours;             // [0,23]
    short hm_minutes;           // [0,59]
};

extern u_short menu_get_current_menu_item(void);
extern void menu_settings_next_item(void);
extern void menu_settings_previous_item(void);
extern void menu_show_settings(void);
extern void menu_handle_settings_input(u_short*);
