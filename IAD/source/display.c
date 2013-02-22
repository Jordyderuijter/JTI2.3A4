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
static int display_mode = 0;                // The current display mode. 0=Main, 1=Settings menu, 2=timezone setup
static int lcd_backlight_time = 0;          // Used for temporarily lighting up the display. (time in ~500ms/half seconds)

/*-------------------------------------------------------------------------*/
/* local routines (prototyping)                                            */
/*-------------------------------------------------------------------------*/
static void LcdWriteByte(u_char, u_char);
static void LcdWriteNibble(u_char, u_char);
static void LcdWaitBusy(void);

void _display_main_screen(void);
void _display_timezone_setup(void);

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

                        //'0' + (tm->tm_sec / 10), '0' + (tm->tm_sec % 10),
                        ' ', ' ',

                        '0' + (tm->tm_mday / 10), '0' + (tm->tm_mday % 10), '-',
                        '0' + (tm->tm_mon / 10), '0' + (tm->tm_mon % 10), '-',
                        '0' + ((tm->tm_year % 100) / 10) - 1, '0' + (tm->tm_year % 10), '\0'};
    lcd_display_string(string);
}

/**
 * Displays the given string on the LCD display, starting at the current cursor position.
 * @param string The string to display
 */
void lcd_display_string(char * string)
{
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

void lcd_display_main_screen()
{
    display_mode = 0;
}

/**
 * Changes 'displaymode' to display the settings menu.
 */
void lcd_display_settings_menu()
{
    display_mode = 1;
}

/**
 * Starting point of the thread that handles everything on the LCD screen.
 * @param arg
 */
THREAD(DisplayThread, arg)
{
    tm time_stamp;
    
    for(;;)
    {
        // First display the 'constant' information at each update. This doesn't HAVE to be done every update, but let's do it anyway just to be sure.
        lcd_cursor_home();     // Can probably be removed, first needs testing though!
        X12RtcGetClock(&time_stamp);
        lcd_display_timestamp(&time_stamp);
        
        // Turn off the backlight if it was temporarily enabled.
        if(lcd_backlight_time == 0)
            LcdBackLight(LCD_BACKLIGHT_OFF);
        else
            lcd_backlight_time--;   // Decrease the backlight 'timer'.
        
        // Display the 'variable' information at each update, based on the value of display_mode.
        switch(display_mode)
        {
            case 0:
                _display_main_screen();
                break;
            case 1:
                menu_show_settings();
                break;
            case 2:
                _display_timezone_setup();
                break;
        }
        
        NutSleep(500);
    }
}

/**
 * Shows the main screen (radio/rss information)
 */
void _display_main_screen()
{
    display_mode = 0;
    // Display radio/RSS info here.
}

/**
 * Shows the timezone setup screen.
 */
void lcd_display_timezone_setup()
{
    display_mode = 2;
}

/**
 * Shows the timezone setup.
 */
void _display_timezone_setup()
{
    lcd_display_string("Timezone:");
}

/* ---------- end of module ------------------------------------------------ */

/*@}*/
