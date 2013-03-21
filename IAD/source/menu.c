/**
 * Module that handles the displaying the menus.
 */

#include "display.h"
#include "keyboard.h"
#include "menu.h"
#include "rtc.h"
#include "log.h"
#include "flash.h"
#include <time.h> 

#define LOG_MODULE  LOG_MAIN_MODULE
#define MAX_MENU_ITEM_INDEX 3          // Zero-based index

// 'LOCAL VARIABLE DEFINITIONS'
short menu_item = 0;           // Menu item to display. 0 = A time, 1 = A snooze, 2 = B time, 3 = B date.
u_int information_changed = 1;

void menu_show_settings(void);
void menu_settings_next_item(void);
void menu_settings_previous_item(void);
void menu_handle_settings_input(u_short* input_mode);
void menu_lcd_display_information(tm *p_alarm_a, tm *p_alarm_b, int snooze_interval, int menu_item);

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
    static tm alarm_a;
    static tm* p_alarm_a = &alarm_a;
    static tm alarm_b;
    static tm* p_alarm_b = &alarm_b;
    static u_short cursor_position = 3;
    static short snooze_interval;
    static short button_cooldown_counter = 0; 
    static bool button_cooldown = true;
    static bool first_call = true;
    
    if(first_call)
    {            
        get_alarm_a(p_alarm_a);
        LogMsg_P(LOG_INFO, PSTR("Alarm A time [%d:%d:%d %d-%d-%d]"), p_alarm_a->tm_hour, p_alarm_a->tm_min, p_alarm_a->tm_sec, p_alarm_a->tm_mday, p_alarm_a->tm_mon, p_alarm_a->tm_year );
        get_alarm_b(p_alarm_b);
        LogMsg_P(LOG_INFO, PSTR("Alarm B time [%d:%d:%d %d-%d-%d]"), p_alarm_b->tm_hour, p_alarm_b->tm_min, p_alarm_b->tm_sec, p_alarm_b->tm_mday, p_alarm_b->tm_mon);
        get_snooze(&snooze_interval); 
    }
    
    if(button_cooldown_counter >= 2)
    {
        button_cooldown = false;
        button_cooldown_counter = 0;
    }
    
    if(!button_cooldown)
    {
        switch(kb_button_pressed())
        {
            case KEY_ESC:       
                if(!in_edit_mode)
                {
                    *input_mode = 0;
                    lcd_refresh_information(); //RSS / Radio information should be shown here. 
                    lcd_display_main_screen();
                    menu_item = 0;
                    first_call=true; 
                    return;
                }
                break;
            
            case KEY_OK:       
                if(in_edit_mode)
                {
                    switch (menu_item)
                    {
                        case 0: 
                            set_alarm_a(p_alarm_a);
                            At45dbPageWrite(3, p_alarm_a, sizeof(tm));
                            break;
                        case 1:
                            At45dbPageWrite(2, &snooze_interval, 2);
                            break;
                        case 2:                            
                        case 3:
                            set_alarm_b(p_alarm_b);
                            break;
                    }
                }
                in_edit_mode = !in_edit_mode;
                lcd_show_cursor(in_edit_mode);
                break;
                
            case KEY_UP:      
                if(in_edit_mode)
                {
                    switch(menu_item)
                    {
                        case 0:                     
                            if(cursor_position == 3)
                            {
                                p_alarm_a->tm_hour++;

                                if(p_alarm_a->tm_hour > 23)
                                    p_alarm_a->tm_hour = 0;
                            }
                            else
                            {
                                p_alarm_a->tm_min++;

                                if(p_alarm_a->tm_min > 59)
                                    p_alarm_a->tm_min = 0;
                            }
                            break;
                            
                        case 1:                      
                            snooze_interval++;
                            if(snooze_interval > 120)
                                snooze_interval = 0;
                            break;
                            
                        case 2:                   
                            if(cursor_position == 3)
                            {
                                p_alarm_b->tm_hour++;

                                if(p_alarm_b->tm_hour > 23)
                                    p_alarm_b->tm_hour = 0;
                            }
                            else
                            {
                                p_alarm_b->tm_min++;

                                if(p_alarm_b->tm_min > 59)
                                    p_alarm_b->tm_min = 0;
                            }
                            break;
                            
                        case 3:
                            if(cursor_position == 3)
                            {
                                p_alarm_b->tm_mday++;              
                                if(p_alarm_b->tm_mday > 31)
                                    p_alarm_b->tm_mday = 0;
                            }
                            else if(cursor_position == 6)
                            {
                                p_alarm_b->tm_mon++;           
                                if(p_alarm_b->tm_mon > 11)
                                    p_alarm_b->tm_mon = 0;
                            }
                            break;
                            
                        default:
                            break;                     
                    }
                }
                else
                {
                    menu_settings_previous_item();
                    if(menu_item == 1)
                        cursor_position = 9;
                    else
                        cursor_position = 3; 
                }  
                break;
                
            case KEY_DOWN:   
                if(in_edit_mode)
                {
                    switch(menu_item)
                    {
                        case 0:
                            if(cursor_position == 3)
                            {
                                p_alarm_a->tm_hour--;

                                if(p_alarm_a->tm_hour < 0)
                                    p_alarm_a->tm_hour = 23;
                            }
                            else
                            {
                                p_alarm_a->tm_min--;

                                if(p_alarm_a->tm_min< 0)
                                    p_alarm_a->tm_min = 59;
                            }
                            break;
                            
                        case 1:     
                            snooze_interval--;
                            if(snooze_interval < 0)
                                snooze_interval = 120;
                            break;
                            
                        case 2:     
                            if(cursor_position == 3)
                            {
                                p_alarm_b->tm_hour--;              
                                if(p_alarm_b->tm_hour < 0)
                                    p_alarm_b->tm_hour = 23;
                            }
                            else
                            {
                                p_alarm_b->tm_min--;       
                                if(p_alarm_b->tm_min < 0)
                                    p_alarm_b->tm_min = 59;
                            }
                            break;
                            
                        case 3:     
                            if(cursor_position == 3)
                            {
                                p_alarm_b->tm_mday--;              
                                if(p_alarm_b->tm_mday < 1)
                                    p_alarm_b->tm_mday = 31;
                            }
                            else if(cursor_position == 6)
                            {
                                p_alarm_b->tm_mon--;           
                                if(p_alarm_b->tm_mon < 0)
                                    p_alarm_b->tm_mon = 11;
                            }                         
                            break;
                            
                         default:
                            break;
                    }
                }
                else
                {
                    menu_settings_next_item();           
                    if(menu_item == 1)
                        cursor_position = 9;
                    else
                        cursor_position = 3;                  
                }
                break;
                
            case KEY_LEFT:
                switch(menu_item)
                {                        
                    case 0:  
                    case 2:
                    case 3:
                        if(cursor_position == 3)
                            cursor_position = 6;
                        else 
                            cursor_position = 3;
                        break;
                }
                break;
                
            case KEY_RIGHT:             
                switch(menu_item)
                {
                    case 0:                             // Alarm A time
                    case 2:                             // Alarm B time
                    case 3:                             // Alarm B date
                        if(cursor_position == 3)
                            cursor_position = 6;
                        else
                            cursor_position = 3;
                        break;                 
                }      
                break;         
            default:
                break;
        }
        if((kb_get_buttons_pressed_raw() ^ 0xFFFF) != 0)
        {
            button_cooldown = true;       
            // Handles the display of the settings menu on the LCD screen. Only refesh if anything has changed.
            menu_lcd_display_information(p_alarm_a, p_alarm_b, snooze_interval, menu_item);   
        }             
    }  
    if(first_call)
    {
        menu_lcd_display_information(p_alarm_a, p_alarm_b, snooze_interval, menu_item); 
        first_call = false;
    }
    
    if(button_cooldown)
        button_cooldown_counter++; 
    if(in_edit_mode)
        lcd_place_cursor_at(cursor_position, 1);
}

void menu_lcd_display_information(tm *p_alarm_a, tm *p_alarm_b, int snooze_interval, int menu_item)
{
    char display_string[13];
    switch(menu_item)
    {
        case 0:
            display_string[0] = 'A';
            display_string[1] = ':';
            display_string[2] = '0' + p_alarm_a->tm_hour / 10;
            display_string[3] = '0' + p_alarm_a->tm_hour % 10;
            display_string[4] = ':';
            display_string[5] = '0' + p_alarm_a->tm_min / 10;
            display_string[6] = '0' + p_alarm_a->tm_min % 10;
            int x = 0;
            for(x = 7; x < 13; x++)
                display_string[x] = ' ';
            display_string[13] = '\0';
            break;
    
        case 1:
            display_string[0] = 'S';
            display_string[1] = 'n';
            display_string[2] = 'o';
            display_string[3] = 'o';
            display_string[4] = 'z';
            display_string[5] = 'e';
            display_string[6] = ':';          
            display_string[7] = '0' + snooze_interval / 100;
            display_string[8] = '0' + snooze_interval %100 / 10;
            display_string[9] = '0' + snooze_interval %10;    
            display_string[10] = 'm';
            display_string[11] = 'i';
            display_string[12] = 'n';   
            display_string[13] = '\0';
            break;
         
        case 2:
            display_string[0] = 'B';
            display_string[1] = ':';
            display_string[2] = '0' + p_alarm_b->tm_hour / 10;
            display_string[3] = '0' + p_alarm_b->tm_hour % 10;
            display_string[4] = ':';
            display_string[5] = '0' + p_alarm_b->tm_min / 10;
            display_string[6] = '0' + p_alarm_b->tm_min % 10;
            int y = 0;
            for(y = 7; y < 13; y++)
                display_string[y] = ' ';
            display_string[13] = '\0';
            break;
            
        case 3:
            display_string[0] = 'B';
            display_string[1] = ':';
            display_string[2] = '0' + p_alarm_b->tm_mday / 10;
            display_string[3] = '0' + p_alarm_b->tm_mday % 10;
            display_string[4] = '-';
            display_string[5] = '0' + p_alarm_b->tm_mon / 9 ;
            if(p_alarm_b->tm_mon / 9 == 1)
                display_string[6] = '0' + p_alarm_b->tm_mon % 9 ;
            else
                display_string[6] = '1' + p_alarm_b->tm_mon % 10;
            int z = 0;
            for(z = 7; z < 13; z++)
                display_string[z] = ' '; 
            display_string[13] = '\0';
            break;
    }
    lcd_set_information(display_string);
    information_changed = 1; 
}
