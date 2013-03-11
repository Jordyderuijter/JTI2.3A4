/**
 * Module that handles the displaying the menus.
 */

#include "display.h"
#include "keyboard.h"
#include "menu.h"
#include "rtc.h"
#include <time.h> 
#include "flash.h"

#define MAX_MENU_ITEM_INDEX 3          // Zero-based index

// 'LOCAL VARIABLE DEFINITIONS'
short menu_item = 0;           // Menu item to display. 0 = A time, 1 = A snooze, 2 = B time, 3 = B date.

void menu_show_settings(void);
void menu_settings_next_item(void);
void menu_settings_previous_item(void);
void menu_handle_settings_input(u_short* input_mode);
void menu_lcd_display_information(struct hm *p_alarm_a, tm *p_alarm_b, int snooze_interval, int menu_item);

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
    static tm alarm_b;
    static tm* p_alarm_b = &alarm_b;
    static u_short cursor_position = 3;
    static short snooze_interval;
    static short button_cooldown_counter = 0; 
    static bool button_cooldown = true;
    static bool first_call = true;
    
    // These should be heap memory!
    tm alarm_a_time;
    tm* p_alarm_a_time = &alarm_a_time;
    tm alarm_b_original;
    tm* p_alarm_b_original = &alarm_b_original;
    
    if(first_call)
    {
        p_alarm_b->tm_mday = 0;
        p_alarm_b->tm_mon = 0;
        p_alarm_b->tm_year = 2013;
        p_alarm_b->tm_hour = 0;
        p_alarm_b->tm_min = 0;
        p_alarm_b->tm_sec = 0;
        first_call = false;
    }
    
    if(button_cooldown_counter >= 2)
    {
        button_cooldown = false;
        button_cooldown_counter = 0;
    }
    
    if(kb_button_is_pressed(KEY_ESC) && !in_edit_mode && !button_cooldown)
    {
        button_cooldown = true;
        *input_mode = 0;
        
        lcd_show_cursor(false);
        lcd_display_main_screen();
        menu_item = -1;
        
        return;
    }
    else if(kb_button_is_pressed(KEY_OK) && !button_cooldown)
    {
        // Save data
        if(in_edit_mode)
        {
            switch(menu_item)
            {
                case 0:
                    p_alarm_a_time->tm_min = p_alarm_a->hm_minutes;
                    p_alarm_a_time->tm_hour = p_alarm_a->hm_hours;
                    set_alarm_a(p_alarm_a_time);
                    break;
                case 1:
                    At45dbPageWrite(2, &snooze_interval, sizeof(short));
                    break;
                case 2:
                    get_alarm_b(p_alarm_b_original);
                    p_alarm_b->tm_mday = p_alarm_b_original->tm_mday;
                    p_alarm_b->tm_mon = p_alarm_b_original->tm_mon;
                    p_alarm_b->tm_year = p_alarm_b_original->tm_year;
                    set_alarm_b(p_alarm_b);
                    break;
                case 3:
                    get_alarm_b(p_alarm_b_original);
                    p_alarm_b->tm_min = p_alarm_b_original->tm_min;
                    p_alarm_b->tm_hour = p_alarm_b_original->tm_hour;
                    set_alarm_b(p_alarm_b);
                    break;
            }
        }
        
        button_cooldown = true;
        in_edit_mode = !in_edit_mode;
        lcd_show_cursor(in_edit_mode);    
    }
    else if(kb_button_is_pressed(KEY_UP) && !button_cooldown)
    {
        button_cooldown = true;
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
        else if(menu_item == 1 && in_edit_mode)
        {    
            snooze_interval++;
            if(snooze_interval > 120)
                snooze_interval = 0;
        }
        else if(menu_item == 2 && in_edit_mode)
        {
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
        }
        else if(menu_item == 3 && in_edit_mode)
        {
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
            else 
            {
                p_alarm_b->tm_year++; 
            }
        } 
        else
            menu_settings_previous_item();
    }
    else if(kb_button_is_pressed(KEY_DOWN) && !button_cooldown)
    {
        button_cooldown = true;
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
        else if(menu_item == 1 && in_edit_mode)
        {       
            snooze_interval--;
            if(snooze_interval < 0)
                snooze_interval = 120;
        }
        else if(menu_item == 2 && in_edit_mode)
        {
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
        }
        else if(menu_item == 3 && in_edit_mode)
        {
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
            else 
            {
                p_alarm_b->tm_year--;          
            }
        }
        else
        {
            menu_settings_next_item();
        }
    }
    else if(kb_button_is_pressed(KEY_LEFT) && !button_cooldown)
    {
        button_cooldown = true;
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
                if(cursor_position == 3)
                    cursor_position = 9;
                else if(cursor_position == 9)
                    cursor_position = 6;
                else 
                    cursor_position = 3;
                break;
        }
    }
    else if(kb_button_is_pressed(KEY_RIGHT) && !button_cooldown)
    {
        button_cooldown = true;     
        switch(menu_item)
        {
            case 0:                             // Alarm A time
            case 2:                             // Alarm B time
                if(cursor_position == 3)
                    cursor_position = 6;
                else
                    cursor_position = 3;
                break;
            case 3:                             // Alarm B date
                if(cursor_position == 3)
                    cursor_position = 6;
                else if(cursor_position == 6)
                    cursor_position = 9;
                else 
                    cursor_position = 3;
                break;
        }
    }
    
    // Handles the display of the settings menu on the LCD screen.
    menu_lcd_display_information(p_alarm_a, p_alarm_b, snooze_interval, menu_item);  
   
    if(button_cooldown)
        button_cooldown_counter++;
    
    lcd_place_cursor_at(cursor_position, 1);
}

void menu_lcd_display_information(struct hm *p_alarm_a, tm *p_alarm_b, int snooze_interval, int menu_item)
{
    char display_string[13];
    switch(menu_item)
    {
        case 0:
            display_string[0] = 'A';
            display_string[1] = ':';
            display_string[2] = '0' + p_alarm_a->hm_hours / 10;
            display_string[3] = '0' + p_alarm_a->hm_hours % 10;
            display_string[4] = ':';
            display_string[5] = '0' + p_alarm_a->hm_minutes / 10;
            display_string[6] = '0' + p_alarm_a->hm_minutes % 10;
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
                display_string[6] = '1' + p_alarm_b->tm_mon % 10 ;
            display_string[7] = '-';  
            display_string[8] = '0' + p_alarm_b->tm_year / 1000;
            display_string[9] = '0' + p_alarm_b->tm_year % 1000 / 100;
            display_string[10] = '0' + p_alarm_b->tm_year % 100 / 10;
            display_string[11] = '0' + p_alarm_b->tm_year % 10;
            display_string[12] = ' '; 
            display_string[13] = '\0';
            break;
    }
    lcd_set_information(display_string);
}
