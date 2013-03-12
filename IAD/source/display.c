/* ========================================================================
 * [PROJECT]    SIR100
 * [MODULE]     Display
 * [TITLE]      display source file
 * [FILE]       display.c
 * [VSN]        1.0
 * [CREATED]    26092003
 * [LASTCHNGD]  06102006
 * [COPYRIGHT]  Copyright (C) STREAMIT BV
 * [PURPOSE]    contains all interface- and low-level routines to
 *              control the LCD and write characters or strings (menu-items)
 * ======================================================================== */

#define LOG_MODULE  LOG_DISPLAY_MODULE

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/timer.h>
#include <sys/event.h>
#include <sys/thread.h>
#include <sys/heap.h>

#include "system.h"
#include "portio.h"
#include "display.h"
#include "log.h"
#include "menu.h"
#include <stdbool.h>
#include "rtc.h"

/*-------------------------------------------------------------------------*/
/* local defines                                                           */
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/* local variable definitions                                              */
/*-------------------------------------------------------------------------*/
//static int display_mode = 0;                // The current display mode. 0=Main, 1=Settings menu, 2=timezone setup
static int lcd_backlight_time = 0;          // Used for temporarily lighting up the display. (time in ~500ms/half seconds)
static int offset = 0;                      // The offset of the information scrolling.
static char information[] = "";             // The information displayed on the LCD screen.
bool alarmstatus_changed = true;


/*-------------------------------------------------------------------------*/
/* local routines (prototyping)                                            */
/*-------------------------------------------------------------------------*/
void LcdBackLight(u_char Mode);
void LcdChar(char MyChar);
void LcdLowLevelInit(void);
static void LcdWriteByte(u_char, u_char);
static void LcdWriteNibble(u_char, u_char);
static void LcdWaitBusy(void);
void lcd_clear(void);
void lcd_cursor_home(void);
void lcd_display_timestamp(struct _tm* tm);
int lcd_display_string_at(char* string, int x, int y);
void lcd_backlight_on(int time);
void lcd_display_alarmstatus(bool alarmA, bool alarmB);
void lcd_display_information(void);
void lcd_set_information(char *tmp_information);
void lcd_display_main_screen(void);
void _display_main_screen(void);
void lcd_display_timezone_setup(void);
void lcd_show_cursor(bool value);
int lcd_place_cursor_at(int x, int y);


/*!
 * \addtogroup Display
 */

/*@{*/

/*-------------------------------------------------------------------------*/
/*                         start of code                                   */
/*-------------------------------------------------------------------------*/

/* ����������������������������������������������������������������������� */
/*!
 * \brief control backlight
 */
/* ����������������������������������������������������������������������� */
void LcdBackLight(u_char Mode)
{
    if (Mode==LCD_BACKLIGHT_ON)
    {
        sbi(LCD_BL_PORT, LCD_BL_BIT);   // Turn on backlight
    }

    if (Mode==LCD_BACKLIGHT_OFF)
    {
        cbi(LCD_BL_PORT, LCD_BL_BIT);   // Turn off backlight
    }
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Write a single character on the LCD
 *
 * Writes a single character on the LCD on the current cursor position
 *
 * \param LcdChar character to write
 */
/* ����������������������������������������������������������������������� */
void LcdChar(char MyChar)
{
    LcdWriteByte(WRITE_DATA, MyChar);
}


/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level initialisation function of the LCD-controller
 *
 * Initialise the controller and send the User-Defined Characters to CG-RAM
 * settings: 4-bit interface, cursor invisible and NOT blinking
 *           1 line dislay, 10 dots high characters
 *
 */
/* ����������������������������������������������������������������������� */
 void LcdLowLevelInit()
{
    u_char i;

    NutDelay(140);                               // wait for more than 140 ms after Vdd rises to 2.7 V

    for (i=0; i<3; ++i)
    {
        LcdWriteNibble(WRITE_COMMAND, 0x33);      // function set: 8-bit mode; necessary to guarantee that
        NutDelay(4);                              // SIR starts up always in 5x10 dot mode
    }

    LcdWriteNibble(WRITE_COMMAND, 0x22);        // function set: 4-bit mode; necessary because KS0070 doesn't
    NutDelay(1);                                // accept combined 4-bit mode & 5x10 dot mode programming

    //LcdWriteByte(WRITE_COMMAND, 0x24);        // function set: 4-bit mode, 5x10 dot mode, 1-line
    LcdWriteByte(WRITE_COMMAND, 0x28);          // function set: 4-bit mode, 5x7 dot mode, 2-lines
    NutDelay(5);

    LcdWriteByte(WRITE_COMMAND, 0x0C);          // display ON/OFF: display ON, cursor OFF, blink OFF
    NutDelay(5);

    LcdWriteByte(WRITE_COMMAND, 0x01);          // display clear
    NutDelay(5);

    LcdWriteByte(WRITE_COMMAND, 0x06);          // entry mode set: increment mode, entire shift OFF
    LcdWriteByte(WRITE_COMMAND, 0x80);          // DD-RAM address counter (cursor pos) to '0'
}


/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level routine to write a byte to LCD-controller
 *
 * Writes one byte to the LCD-controller (by  calling LcdWriteNibble twice)
 * CtrlState determines if the byte is written to the instruction register
 * or to the data register.
 *
 * \param CtrlState destination: instruction or data
 * \param LcdByte byte to write
 *
 */
/* ����������������������������������������������������������������������� */
static void LcdWriteByte(u_char CtrlState, u_char LcdByte)
{
    LcdWaitBusy();                      // see if the controller is ready to receive next byte
    LcdWriteNibble(CtrlState, LcdByte & 0xF0);
    LcdWriteNibble(CtrlState, LcdByte << 4);

}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level routine to write a nibble to LCD-controller
 *
 * Writes a nibble to the LCD-controller (interface is a 4-bit databus, so
 * only 4 databits can be send at once).
 * The nibble to write is in the upper 4 bits of LcdNibble
 *
 * \param CtrlState destination: instruction or data
 * \param LcdNibble nibble to write (upper 4 bits in this byte
 *
 */
/* ����������������������������������������������������������������������� */
static void LcdWriteNibble(u_char CtrlState, u_char LcdNibble)
{
    outp((inp(LCD_DATA_DDR) & 0x0F) | 0xF0, LCD_DATA_DDR);  // set data-port to output again

    outp((inp(LCD_DATA_PORT) & 0x0F) | (LcdNibble & 0xF0), LCD_DATA_PORT); // prepare databus with nibble to write

    if (CtrlState == WRITE_COMMAND)
    {
        cbi(LCD_RS_PORT, LCD_RS);     // command: RS low
    }
    else
    {
        sbi(LCD_RS_PORT, LCD_RS);     // data: RS high
    }

    sbi(LCD_EN_PORT, LCD_EN);

    asm("nop\n\tnop");                    // small delay

    cbi(LCD_EN_PORT, LCD_EN);
    cbi(LCD_RS_PORT, LCD_RS);
    outp((inp(LCD_DATA_DDR) & 0x0F), LCD_DATA_DDR);           // set upper 4-bits of data-port to input
    outp((inp(LCD_DATA_PORT) & 0x0F) | 0xF0, LCD_DATA_PORT);  // enable pull-ups in data-port
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Low-level routine to see if the controller is ready to receive
 *
 * This routine repeatetly reads the databus and checks if the highest bit (bit 7)
 * has become '0'. If a '0' is detected on bit 7 the function returns.
 *
 */
/* ����������������������������������������������������������������������� */
static void LcdWaitBusy()
{
    u_char Busy = 1;
	u_char LcdStatus = 0;

    cbi (LCD_RS_PORT, LCD_RS);              // select instruction register

    sbi (LCD_RW_PORT, LCD_RW);              // we are going to read

    while (Busy)
    {
        sbi (LCD_EN_PORT, LCD_EN);          // set 'enable' to catch 'Ready'

        asm("nop\n\tnop");                  // small delay
        LcdStatus =  inp(LCD_IN_PORT);      // LcdStatus is used elsewhere in this module as well
        Busy = LcdStatus & 0x80;            // break out of while-loop cause we are ready (b7='0')
    }

    cbi (LCD_EN_PORT, LCD_EN);              // all ctrlpins low
    cbi (LCD_RS_PORT, LCD_RS);
    cbi (LCD_RW_PORT, LCD_RW);              // we are going to write
}

/**
 * Clears the LCD display and sets the cursor to [0][0]
 */
void lcd_clear()
{
    LcdWriteByte(WRITE_COMMAND, 0x01);          // Display clear command.
    NutDelay(2);

    lcd_cursor_home();
}

/*
 * Sets the cursor position to [0][0] (top left, first position)
 */
void lcd_cursor_home()
{
    LcdWriteByte(WRITE_COMMAND, 0x02);          // Set cursor to home command.
}

/**
 * Displays the given timestamp in format hh:mm:ss on the display, starting at [0][0]
 * @param tm The timestamp to display on the LCD display
 */
void lcd_display_timestamp(struct _tm* tm)
{    
    char string[] = {    '0' + (tm->tm_hour / 10), '0' + (tm->tm_hour % 10), ':',
                        '0' + (tm->tm_min / 10), '0' + (tm->tm_min % 10),
                        ' ',
                        '0' + (tm->tm_mday / 10), '0' + (tm->tm_mday % 10), '-',
                        '0' + (tm->tm_mon / 10), '0' + (tm->tm_mon % 10), '-',
                        '2', '0' + (tm->tm_year /100)-1, '0' + ((tm->tm_year % 100) / 10), '0' + (tm->tm_year % 10), '\0'};
    lcd_display_string_at(string, 0, 0);
}

/**
 * Displays the given string on the LCD display at the given location.
 * @param string The string to display
 * @param x The x-coordinate of the first letter of the string, ranging from 0 - 15
 * @param y The y-coordinate of the first letter of the string, ranging from 0 - 1
 * @return 0 on success, -1 on failure
 */
int lcd_display_string_at(char* string, int x, int y)
{
    if(x < 0 || x > 15 || y < 0 || y > 1)
        return -1;
    
    // TODO: TEST CURSOR POSITION
    LcdWriteByte(WRITE_COMMAND, 0x80 + (y * 64) + x );          // DD-RAM address counter (cursor pos) to the requested position.
    int i;
    for(i = 0; (string[i] != '\0') && (i < 16 -x); i++)        // Loop through the string and display each character individually.
        LcdChar(string[i]);
    
    return 0;
}

/**
 * Temporarily light up the display.
 * @param time  time in ~500ms/half seconds
 */
void lcd_backlight_on(int time)
{
    if(time > 0)
    {
        LcdBackLight(LCD_BACKLIGHT_ON);
        lcd_backlight_time = time;
    }
}

/*
 * Displays the status of alarm A and alarm B on the bottom right of the screen.
 */
void lcd_display_alarmstatus(bool alarmA, bool alarmB)
{
    if(alarmA) 
    {
        if(alarmB)  
        {
            char alarmStatus[4] = "|AB";
            lcd_display_string_at(alarmStatus, 13, 1);
        }
        else
        {
            char alarmStatus[4] = "|A ";
            lcd_display_string_at(alarmStatus, 13, 1); 
        }
    }
    else
    {
        if(alarmB)
        {
            char alarmStatus[4] = "| B";
            lcd_display_string_at(alarmStatus, 13, 1);
        }
        else
        {
            char alarmStatus[4] = "|  ";
            lcd_display_string_at(alarmStatus, 13, 1); 
        }
    }
    
}

/*
 * Displays the information on the screen, if the information is longer than 13 characters it will scroll.
 */
void lcd_display_information()
{
    int information_size = strlen(information);
    char visibleString[14];
    static int t = 0;
    //If "information" is longer than 13 characters it won't fit on the screen, so it will be scrolled.
    if(information_size > 13)
    {    
        if(t == 10)
        {
            t=0;
            int i;                   
            for(i = 0; i < 13; i++)
            {
                if(!(i+offset >= information_size)) //End of string not reached yet.
                {           
                    visibleString[i] = information[i + offset];
                }
                else                                    //End of string reached.
                {                                    
                    visibleString[i] = information[i + offset - information_size];
                }
            }  
            visibleString[13] = '\0';
            lcd_display_string_at(visibleString, 0, 1); //Display the full string on the LCD.
        
            offset++;                                   //Increase the offset of the string by 1.
            if(offset == information_size -1)           //If the end of the string has been reached, start over at the beginning of the string.
            {
                offset = 0;
            }
        }
    }
    else 
    {
        if(information_changed)
        {
            lcd_display_string_at(information, 0, 1);
            information_changed = 0;
        }
    }
    t++;
}

/*
 * Sets the information which is displayed on the bottom left of the screen.
 */
void lcd_set_information(char *tmp_information)
{
    strcpy(information, tmp_information);
}

/**
 * Changes 'displaymode' to display the 'main screen'.
 */
void lcd_display_main_screen()
{
    tm time_stamp;
    X12RtcGetClock(&time_stamp);
    lcd_display_timestamp(&time_stamp);         //Shows the time and date on first line of the screen.
    if(alarmstatus_changed)                     //Only print if alarmstatus changes to make sure the cursor doesn't bug.
    {
        lcd_display_alarmstatus(alarm_a_on, alarm_b_on); //Shows the status of the alarms on the screen in the bottom right corner.
        alarmstatus_changed = false;
    }
   
}

/**
 * Shows the timezone setup screen.
 */
void lcd_display_timezone_setup()
{
    lcd_clear();
    lcd_display_string_at("Timezone:  00:00\0", 0, 0);
}


void lcd_show_cursor(bool value)
{
    if(value)
        LcdWriteByte(WRITE_COMMAND, 0x0F);  // 0E: underline cursor. Or: 0F for block cursor
    else
        LcdWriteByte(WRITE_COMMAND, 0x0C);
}

/**
 * Place cursor at the given position.
 * @param The x-coordinate on the display (0-15)
 * @param The y-coordinate on the display (0-1)
 * @return 0 on success, -1 on failure
 */
int lcd_place_cursor_at(int x, int y)
{
    if(x < 0 || x > 15 || y < 0 || y > 1)
        return -1;
    
    LcdWriteByte(WRITE_COMMAND, 0x80 + (y * 64) + x );          // DD-RAM address counter (cursor pos) to the requested position.
    
    return 0;
}


/*-------------------------------------------------------------------------*/
/* threads                                                                 */
/*-------------------------------------------------------------------------*/

/**
 * Starting point of the thread that handles everything on the LCD screen except the information.
 * @param arg
 */
THREAD(DisplayThread, arg)
{
    for(;;)
    {
        lcd_display_main_screen(); //Shows time, date and alarm status. Only shows alarm status again if it changed.  
        
        // Turn off the backlight if it was temporarily enabled.
        if(lcd_backlight_time == 0)
            LcdBackLight(LCD_BACKLIGHT_OFF);
        else
            lcd_backlight_time--;   // Decrease the backlight 'timer'.  
        
        NutSleep(500);
    }
}

/**
 * Starting point of the thread that handles the information on the LCD screen.
 * @param arg
 */
THREAD(InformationThread, arg)
{	
    for(;;)
    {    
        NutSleep(50);
        lcd_display_information(); //Display the information on the left bottom of the screen.    
    }
}

/* ---------- end of module ------------------------------------------------ */

/*@}*/
