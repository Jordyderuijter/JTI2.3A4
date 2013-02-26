/**
 * Module that handles the displaying the menus.
 */

#include "display.h"
#include "keyboard.h"
#include "menu.h"
#include "rtc.h"

#define MAX_MENU_ITEM_INDEX 2           // Zero-based index

// 'LOCAL VARIABLE DEFINITIONS'
short menu_item = 0;          // Menu item to display. 0 = A time, 1 = A snooze, 2 = B time, 3 = B date.

// PROTOTYPES
void _show_alarmA_menu_item(void);
void _show_alarmA_snooze_menu_item(void);
void _show_alarmB_menu_item(void);

/**
 * Show the settings item for the time of alarm A.
 */
void _show_alarmA_menu_item()
{
    lcd_display_string_at("A: ", 0, 1);
}

/**
 * Show the settings item for the snooze interval of alarm A.
 */
void _show_alarmA_snooze_menu_item()
{
    lcd_display_string_at("Snooze: ", 0, 1);
}

/**
 * Show the settings item for the date and time of alarm B.
 */
void _show_alarmB_menu_item()
{
    lcd_display_string_at("B: ", 0, 1);
}

/**
 * Opens the settings menu.
 */
void menu_show_settings()
{
    // First display the 'constant' information at each update. This doesn't HAVE to be done every update, but let's do it anyway just to be sure.
    static tm time_stamp;
    if(menu_item < 0)
        menu_item = 0;
    
    X12RtcGetClock(&time_stamp);
    lcd_display_timestamp(&time_stamp);
    
    switch(menu_item)
    {
        case 0:                                 // Show alarm A time setting.
            _show_alarmA_menu_item();
            break;
        case 1:                                 // Show alarm A snooze setting.
            _show_alarmA_snooze_menu_item();
            break;
        case 2:                                 // Show alarm B date + time setting.
            _show_alarmB_menu_item();
            break;
    }
}

/**
 * Show the next menu item.
 */
void menu_settings_next_item()
{
    menu_item++;
    
    if(menu_item > MAX_MENU_ITEM_INDEX)
        menu_item = 0;
}

/**
 * Show the previous menu item.
 */
void menu_settings_previous_item()
{
    menu_item--;
    
    if(menu_item < 0)
        menu_item = MAX_MENU_ITEM_INDEX;
}

/**
 * Handles input in settings menu.
 */
void menu_handle_settings_input(u_short* input_mode)
{
    static bool in_edit_mode = false;
    static struct hm alarm_a;
    static struct hm* p_alarm_a = &alarm_a;
    static u_short cursor_position = 3;
    static char display_string[6];
    
    if(kb_button_is_pressed(KEY_ESC) && !in_edit_mode)
    {
        *input_mode = 0;
        in_edit_mode = false;
        
        lcd_show_cursor(false);
        lcd_display_main_screen();
        menu_item = -1;
        
        return;
    }
    else if(kb_button_is_pressed(KEY_OK))
    {
        in_edit_mode = !in_edit_mode;
        
        switch(menu_item)
        {
            case 0:
                //set_alarm_a(p_alarm_a);
                break;
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
        }
    }
    else if(kb_button_is_pressed(KEY_UP))
    {
        if(menu_item == 0 && in_edit_mode)
        {
            if(cursor_position == 3)
            {
                p_alarm_a->hm_hours++;
                
                if(p_alarm_a->hm_hours > 23)
                    p_alarm_a->hm_hours = 0;
            }
            else
            {
                p_alarm_a->hm_minutes++;
                
                if(p_alarm_a->hm_minutes > 59)
                    p_alarm_a->hm_minutes = 0;
            }
        }
        else
            menu_settings_previous_item();
    }
    else if(kb_button_is_pressed(KEY_DOWN))
    {
        if(menu_item == 0 && in_edit_mode)
        {
            if(cursor_position == 3)
            {
                p_alarm_a->hm_hours--;
                
                if(p_alarm_a->hm_hours < 0)
                    p_alarm_a->hm_hours = 23;
            }
            else
            {
                p_alarm_a->hm_minutes--;
                
                if(p_alarm_a->hm_minutes < 0)
                    p_alarm_a->hm_minutes = 59;
            }
        }
        else
            menu_settings_next_item();
    }
    else if(kb_button_is_pressed(KEY_LEFT))
    {
        switch(menu_item)
        {
            case 0:
            case 2:
                if(cursor_position == 3)
                    cursor_position = 6;
                else
                    cursor_position = 3;
                break;
            case 3:
                // Date
                break;
        }
    }
    else if(kb_button_is_pressed(KEY_RIGHT))
    {
        switch(menu_item)
        {
            case 0:
            case 2:
                if(cursor_position == 3)
                    cursor_position = 6;
                else
                    cursor_position = 3;
                break;
            case 3:
                // Date
                break;
        }
    }
    
    // USE DISPLAY FUNCTION INSTEAD OF THIS IF STATEMENTS!!!
    if(menu_item == 0)
    {
        display_string[0] = '0' + p_alarm_a->hm_hours / 10;
        display_string[1] = '0' + p_alarm_a->hm_hours % 10;
        display_string[2] = ':';
        display_string[3] = '0' + p_alarm_a->hm_minutes / 10;
        display_string[4] = '0' + p_alarm_a->hm_minutes % 10;
        display_string[5] = '\0';

        lcd_display_string_at(display_string, 3, 1);
        lcd_place_cursor_at(cursor_position, 1);
    }
}

