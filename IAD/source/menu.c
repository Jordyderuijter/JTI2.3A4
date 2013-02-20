/**
 * Module that handles the displaying, navigation and behavior of the used menus.
 */

#include "display.h"
#include "keyboard.h"
#include "menu.h"

void _show_alarmA_menu_item(void);
void _show_alarmA_snooze_menu_item(void);
void _show_alarmB_menu_item(void);
void _show_menu_item(int);
void _show_timezone_menu_item(void);

/**
 * Opens the settings menu.
 */
void menu_show_settings()
{
    u_short menu_item = 0;

    while(!kb_is_pressed(KEY_ESC))                      // Exit the settings menu when the ESC key is pressed.
    {
        if(kb_is_pressed(KEY_UP))                       // Navigate to the previous menu item.
        {
            menu_item--;

            if(menu_item == 0)
                menu_item = 3;

            _show_menu_item(menu_item);                 // THIS ISN'T USEFUL SINCE THE LOGIC SHOULD BE HANDLED IN THIS FUNCTION ANYWAY!!! REWRITE!
        }
        else if(kb_is_pressed(KEY_DOWN))                // Navigate to the next menu item.
        {
            menu_item++;

            if(menu_item == 4)
                menu_item = 0;

            _show_menu_item(menu_item);
        }
    }
}

/**
 * Show the settings item for the time of alarm A.
 */
void _show_alarmA_menu_item()
{
    //lcd_display_information("A:");
    lcd_display_string_at("A", 1, 2);
}

/**
 * Show the settings item for the snooze interval of alarm A.
 */
void _show_alarmA_snooze_menu_item()
{
    //lcd_display_information("Snooze:");
    lcd_display_string_at("Snooze", 1, 2);
}

/**
 * Show the settings item for the date and time of alarm B.
 */
void _show_alarmB_menu_item()
{
    //lcd_display_information("B:");
    lcd_display_string_at("B", 1, 2);
}

/**
 * Shows the menu item mapped to the given value.
 * @param menu_item 0: alarm A time setting\n1: alarm A snooze setting\n2: alarm B date/time setting\n3: timezone setting
 */
void _show_menu_item(int menu_item)
{
    switch(menu_item)
    {
        case 0:
            _show_alarmA_menu_item();
            break;
        case 1:
            _show_alarmA_snooze_menu_item();
            break;
        case 2:
            _show_alarmB_menu_item();
            break;
        case 3:
            _show_timezone_menu_item();
            break;
    }
}

/**
 * Show the settings item for the timezone.
 */
void _show_timezone_menu_item()
{
    //lcd_display_information("Timezone:");
    lcd_display_string_at("Timezone", 1, 2);
}