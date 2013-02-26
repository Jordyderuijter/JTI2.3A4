/**
 * Module that handles the displaying the menus.
 */

#include "display.h"
#include "menu.h"

#define MAX_MENU_ITEM_INDEX 2           // Zero-based index

// 'LOCAL VARIABLE DEFINITIONS'
u_short menu_item = 0;          // Menu item to display. See documentation of _show_menu_item for more information.

// PROTOTYPES
void _show_alarmA_menu_item(void);
void _show_alarmA_snooze_menu_item(void);
void _show_alarmB_menu_item(void);

/**
 * Show the settings item for the time of alarm A.
 */
void _show_alarmA_menu_item()
{
    lcd_display_string_at("A:",0,0);
}

/**
 * Show the settings item for the snooze interval of alarm A.
 */
void _show_alarmA_snooze_menu_item()
{
    lcd_display_string_at("Snooze:",0,0);
}

/**
 * Show the settings item for the date and time of alarm B.
 */
void _show_alarmB_menu_item()
{
    lcd_display_string_at("B:",0,0);
}

/**
 * Opens the settings menu.
 */
void menu_show_settings()
{
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
 * Returns the index of the currently displayed menu item.
 * @return Index of the currently displayed menu item.
 */
u_short menu_get_current_menu_item()
{
    return menu_item;
}
