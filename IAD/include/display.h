/* ========================================================================
 * [PROJECT]    SIR100
 * [MODULE]     Display
 * [TITLE]      display header file
 * [FILE]       display.h
 * [VSN]        1.0
 * [CREATED]    030414
 * [LASTCHNGD]  030414
 * [COPYRIGHT]  Copyright (C) STREAMIT BV 2010
 * [PURPOSE]    API and global defines for display module
 * ======================================================================== */

#include <time.h>       // For tm struct
#include <sys/thread.h>
#include <stdbool.h>
#ifndef _Display_H
#define _Display_H


/*-------------------------------------------------------------------------*/
/* global defines                                                          */
/*-------------------------------------------------------------------------*/
#define DISPLAY_SIZE                16
#define NROF_LINES                  2
#define MAX_SCREEN_CHARS            (NROF_LINES*DISPLAY_SIZE)

#define LINE_0                      0
#define LINE_1                      1

#define FIRSTPOS_LINE_0             0
#define FIRSTPOS_LINE_1             0x40


#define LCD_BACKLIGHT_ON            1
#define LCD_BACKLIGHT_OFF           0

#define ALL_ZERO          			0x00      // 0000 0000 B
#define WRITE_COMMAND     			0x02      // 0000 0010 B
#define WRITE_DATA        			0x03      // 0000 0011 B
#define READ_COMMAND      			0x04      // 0000 0100 B
#define READ_DATA         			0x06      // 0000 0110 B


/*-------------------------------------------------------------------------*/
/* typedefs & structs                                                      */
/*-------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*  Global variables                                                        */
/*--------------------------------------------------------------------------*/
extern bool alarmstatus_changed;
extern bool show_rss;
/*-------------------------------------------------------------------------*/
/* export global routines (interface)                                      */
/*-------------------------------------------------------------------------*/
extern void LcdBackLight(u_char);
extern void LcdChar(char);
extern void LcdLowLevelInit(void);
extern void lcd_clear(void);
extern void lcd_cursor_home(void);
extern void lcd_display_timestamp(tm* tm);
extern int lcd_display_string_at(char*, int, int);
extern void lcd_backlight_on(int);
extern void lcd_display_alarmstatus(bool, bool);
extern void lcd_display_information(void);
extern void lcd_set_information(char *);
extern void lcd_set_rss_information(char *);
extern void lcd_set_radio_information(char *);
extern void lcd_refresh_information(void);
extern void lcd_display_main_screen(void);
extern void lcd_display_settings_menu(void);
extern void _display_main_screen(void);
extern void lcd_display_timezone_setup(void);
extern void lcd_show_cursor(bool);
extern int lcd_place_cursor_at(int x, int y);
extern char *inet_ntoa(u_long addr);

THREAD(DisplayThread, arg);
THREAD(InformationThread, arg);

#endif /* _Display_H */
/*  ����  End Of File  �������� �������������������������������������������� */

