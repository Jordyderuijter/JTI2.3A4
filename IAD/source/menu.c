/**
 * Module that handles the displaying, navigation and behavior of the used menus.
 */

#include "display.h"
#include "keyboard.h"
#include "menu.h"

// lcd_display_information(char[] *);

void menu_open_settings()
{
    u_short menuItem = 0;

    while(!kb_is_pressed(KEY_ESC))
    {
        if(kb_get_buttons_pressed_raw() != 0xFFFF)
        {
            if(kb_is_pressed(KEY_UP))
            {
                menuItem--;
                
                if(menuItem == 0)
                    menuItem = 3;
            }
            else if(kb_is_pressed(KEY_DOWN))
            {
                menuItem++;
                
                if(menuItem == 4)
                    menuItem = 0;
            }
        }
       
        switch(menuItem)
        {
            case 0:
                lcd_display_information("A:");
                break;
            case 1:
                lcd_display_information("Snooze:");
                break;
            case 2:
                lcd_display_information("B:");
                break;
            case 3:
                lcd_display_information("Timezone:");
                break;
        }
    }
}