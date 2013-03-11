/*! \mainpage SIR firmware documentation
 *
 *  \section intro Introduction
 *  A collection of HTML-files has been generated using the documentation in the sourcefiles to
 *  allow the developer to browse through the technical documentation of this project.
 *  \par
 *  \note these HTML files are automatically generated (using DoxyGen) and all modifications in the
 *  documentation should be done via the sourcefiles.
 */

/*! \file
 *  COPYRIGHT (C) STREAMIT BV 2010
 *  \date 19 december 2003
 */
 
#define LOG_MODULE  LOG_MAIN_MODULE

//#define RESET   // If defined, this will reset the 'first time setup status'.
                //Should only be used when uploading code without this defined immediately afterwards!!
//#define USE_INTERNET  // If defined, this will enable NTP synchronization. Should always be defined in final 'production' code.
                        // Only comment when testing without access to an internet connection!
/*--------------------------------------------------------------------------*/
/*  Include files                                                           */
/*--------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/version.h>
#include <dev/irqreg.h>

#include "system.h"
#include "portio.h"
#include "display.h"
#include "remcon.h"
#include "keyboard.h"
#include "led.h"
#include "log.h"
#include "uart0driver.h"
#include "mmc.h"
#include "watchdog.h"
#include "flash.h"
#include "spidrv.h"

#include <time.h>
#include "rtc.h"
#include "menu.h"
#include "vs10xx.h"

// Added by JTI2.3A4
#include <stdbool.h>
#include "board.h"
#include <sys/confnet.h>
#include <io.h>
#include <arpa/inet.h>
#include <pro/dhcp.h>
//#include "inet.h"


/*-------------------------------------------------------------------------*/
/* global variable definitions                                             */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/* local variable definitions                                              */
/*-------------------------------------------------------------------------*/
static u_short input_mode = 0; // Determines which 'input mode' is used. 0=mainscreen, 1=settings menu, 2=timezone setup.

/*-------------------------------------------------------------------------*/
/* local routines (prototyping)                                            */
/*-------------------------------------------------------------------------*/
static void SysMainBeatInterrupt(void*);
void SysInitIO(void);
static void SysControlMainBeat(u_char);

void _main_init(void);
void _handle_timezone_setup_input(void);
void _handle_mainscreen_input(void);

tm* get_ntp_time(float);
void connect_to_internet(void);         // USE THESE TWO FUNCTIONS FROM inet.h AFTER MAKEFILE CAN BE EDITED

/*-------------------------------------------------------------------------*/
/* Stack check variables placed in .noinit section                         */
/*-------------------------------------------------------------------------*/

/*!
 * \addtogroup System
 */

/*@{*/


/*-------------------------------------------------------------------------*/
/*                         start of code                                   */
/*-------------------------------------------------------------------------*/

/* ����������������������������������������������������������������������� */
/*!
 * \brief Main entry of the SIR firmware
 *
 * All the initializations before entering the for(;;) loop are done BEFORE
 * the first key is ever pressed. So when entering the Setup (POWER + VOLMIN) some
 * initializations need to be done again when leaving the Setup because new values
 * might be current now
 *
 * \return \b never returns
 */
/* ����������������������������������������������������������������������� */
int main(void)
{
    bool is_first_startup = true;
    
    _main_init();
    
#ifdef RESET
    At45dbPageWrite(0, &is_first_startup, 1);
#endif
    
#ifndef RESET
    
    // If this is the first startup EVER, show the timezone setup.
    At45dbPageRead(0, &is_first_startup, 1);
    NutSleep(1000);     // Sleep is required to be able to read the ESC key below.
    
    if(kb_button_is_pressed(KEY_ESC) || is_first_startup)        // Go to timezone setup
    {
        lcd_display_timezone_setup();           // RESETS CUR SOR, NEEDS FIXING!     
        lcd_show_cursor(true);
        input_mode = 2;
        if(is_first_startup)
        {
            is_first_startup = false;
            At45dbPageWrite(0, &is_first_startup, 1);
        }
    }
#endif
    
    //Stay in startup screen and don't start threads yet.
    while(input_mode == 2)
    {
        NutSleep(200);
        _handle_timezone_setup_input();
        
        // If a key is pressed, light up the LCD screen.
        if((kb_get_buttons_pressed_raw() ^ 0xFFFF) != 0)
            lcd_backlight_on(20);
    }
    NutThreadCreate("DisplayThread", DisplayThread, NULL, 512);                 // Start thread that handles the displaying on the LCD.
    NutThreadCreate("AlarmPollingThread", AlarmPollingThread, NULL, 512);    // Start thread that constantly 'polls' for activated alarms.
    NutThreadCreate("InformationThread", InformationThread, NULL, 512);         // Start thread that handles the information display on the LCD.
    
    for (;;)
    {
        // If a key is pressed, light up the LCD screen.
        if((kb_get_buttons_pressed_raw() ^ 0xFFFF) != 0)
            lcd_backlight_on(20);
        
        // Handle input based on the current input mode.
        switch(input_mode)
        {
            case 0:
                _handle_mainscreen_input();
                break;
            case 1:
                menu_handle_settings_input(&input_mode);
                break;
        }
        
        // Mandatory main loop code.
        WatchDogRestart();
        NutSleep(100); 
    }

    return(0);      // never reached, but 'main()' returns a non-void, so.....
}


/**
 *  Handles input in 'mainscreen mode'
 */
void _handle_mainscreen_input()
{
    if(kb_button_is_pressed(KEY_OK))               // Go to settings menu
    {
        lcd_display_settings_menu();
        input_mode = 1;
    }
}

/**
 * Handles input in 'timezone setup mode';
 */
void _handle_timezone_setup_input()
{
    static int cursor_position = 11;            // 11 = hours, 14 = minutes.
    static struct hm utc_offset;
    static struct hm* p_utc_offset = &utc_offset;
    static char display_string[7];
    bool cursor_position_changed = false;
    
    if(kb_button_is_pressed(KEY_OK))            // Accept the current timezone offset and leave the setup screen.
    {
        tm timestamp;
        X12RtcGetClock(&timestamp);
        //_correct_timestamp_with_timezone(&timestamp, utc_offset);     Can this be done trough NTP??
        
        // Set new time
        timestamp.tm_hour += p_utc_offset->hm_hours;    // Is going to give problems with the day/month/years!!! (28 feb, 22:00 + 8 hours will give 28 feb 06:00?)
        timestamp.tm_min += p_utc_offset->hm_minutes;   // Is going to give problems with the day/month/years!!!
        
        X12RtcSetClock(&timestamp);
        
        At45dbPageWrite(1, p_utc_offset, sizeof(utc_offset));
        
        lcd_show_cursor(false);
        input_mode = 0; // Switch input mode to mainscreen mode.
        
        lcd_display_main_screen();
        
        return;         // Since we have switched input mode we don't need to execute the other code in the function.
    }
    
    // Move cursor between hours and minutes.
    if(kb_button_is_pressed(KEY_LEFT) || kb_button_is_pressed(KEY_RIGHT))
    {
        if(cursor_position == 11)
            cursor_position = 14;
        else
            cursor_position = 11;       
        lcd_place_cursor_at(cursor_position, 0);
    }
    
    if(cursor_position == 11)               // Position of 'ten-hours'
    {
        if(kb_button_is_pressed(KEY_UP))
        {            
            p_utc_offset->hm_hours++;
            
            if(p_utc_offset->hm_hours > 14)
                p_utc_offset->hm_hours = -12;
            lcd_display_string_at(display_string, 10, 0);  
        }
        else if(kb_button_is_pressed(KEY_DOWN))
        {                      
            p_utc_offset->hm_hours--;
            
            if(p_utc_offset->hm_hours < -12)
                p_utc_offset->hm_hours = 14;
            lcd_display_string_at(display_string, 10, 0);  
        }
    }
    else        // Set the minutes in 15 minutes/button press.
    {
        if(kb_button_is_pressed(KEY_UP))
        {            
            p_utc_offset->hm_minutes += 15;
            
            if(p_utc_offset->hm_minutes >= 60)
                p_utc_offset->hm_minutes = 0;
            lcd_display_string_at(display_string, 10, 0);  
        }
        else if(kb_button_is_pressed(KEY_DOWN))
        {            
            p_utc_offset->hm_minutes -= 15;
            
            if(p_utc_offset->hm_minutes < 0)
                p_utc_offset->hm_minutes = 45;
            lcd_display_string_at(display_string, 10, 0);  
        }
    }
    
    if(p_utc_offset->hm_hours > 0)
        display_string[0] = '+';
    else if(p_utc_offset->hm_hours < 0)
        display_string[0] = '-';
    else
        display_string[0] = ' ';
    
    display_string[1] = '0' + abs(p_utc_offset->hm_hours) / 10;
    display_string[2] = '0' + abs(p_utc_offset->hm_hours) % 10;
    display_string[3] = ':';
    display_string[4] = '0' + p_utc_offset->hm_minutes / 10;
    display_string[5] = '0' + p_utc_offset->hm_minutes % 10;
    display_string[6] = '\0';
    
   
    if(cursor_position_changed)
    {
        lcd_place_cursor_at(cursor_position, 0);
    }
}

// USE THIS FUNCTION FROM inet.h WHEN MAKEFILE CAN BE EDITED!
tm* get_ntp_time(float timezone_offset)
{
    time_t ntp_time = 0;
    tm *ntp_datetime;
    uint32_t timeserver = 0;
    
    _timezone = -1 * 60 * 60;
 
    puts("Retrieving time");
 
    timeserver = inet_addr("193.67.79.202");
 
        if (NutSNTPGetTime(&timeserver, &ntp_time) == 0) {
            
        } else {
            NutSleep(1000);
            puts("Failed to retrieve time.");
        }
    ntp_datetime = localtime(&ntp_time);
    //printf("NTP time is: %02d:%02d:%02d\n", ntp_datetime->tm_hour, ntp_datetime->tm_min, ntp_datetime->tm_sec);
    return ntp_datetime;
}

// USE THIS FUNCTION FROM inet.h WHEN MAKEFILE CAN BE EDITED!
void connect_to_internet()
{
    u_long baud = 115200;
 
    NutRegisterDevice(&DEV_DEBUG, 0, 0);
    freopen(DEV_DEBUG_NAME, "w", stdout);
    _ioctl(_fileno(stdout), UART_SETSPEED, &baud);
    puts("Network Configuration...");
 
    if (NutRegisterDevice(&DEV_ETHER, 0, 0)) {
        puts("Registering " DEV_ETHER_NAME " failed.");
    }
    else if (NutDhcpIfConfig(DEV_ETHER_NAME, NULL, 0)) {
        puts("Configuring " DEV_ETHER_NAME " failed.");
    }
    else {
        printf(inet_ntoa(confnet.cdn_ip_addr));
    }
}

/**
 * Do all initializations that were previously done in the main function itself.
 */
void _main_init()
{
    tm gmt;     // Used to LOG the time.
    
    /*
     *  First disable the watchdog
     */
    WatchDogDisable();

    NutDelay(100);

    SysInitIO();
    SPIinit();
    LedInit();
    LcdLowLevelInit();
    Uart0DriverInit();
    Uart0DriverStart();
    LogInit();
    LogMsg_P(LOG_INFO, PSTR("Application now running"));
    CardInit();
    X12Init();
    if (X12RtcGetClock(&gmt) == 0)
    {
		LogMsg_P(LOG_INFO, PSTR("RTC time [%02d:%02d:%02d]"), gmt.tm_hour, gmt.tm_min, gmt.tm_sec );
    }
    At45dbInit();
    RcInit();
    KbInit();
    VsPlayerInit();
    SysControlMainBeat(ON);             // enable 4.4 msecs heartbeat interrupt

    /*
     * Increase our priority so we can feed the watchdog.
     */
    NutThreadSetPriority(1);

    /* Enable global interrupts */
    sei();
    
#ifdef USE_INTERNET
    connect_to_internet();
    ptime = get_ntp_time(0.0);
    LogMsg_P(LOG_INFO, PSTR("NTP time [%02d:%02d:%02d]"), ptime->tm_hour, ptime->tm_min, ptime->tm_sec );
#endif
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief ISR MainBeat Timer Interrupt (Timer 2 for Mega128, Timer 0 for Mega256).
 *
 * This routine is automatically called during system
 * initialization.
 *
 * resolution of this Timer ISR is 4,448 msecs
 *
 * \param *p not used (might be used to pass parms from the ISR)
 */
/* ����������������������������������������������������������������������� */
static void SysMainBeatInterrupt(void *p)
{

    /*
     *  scan for valid keys AND check if a MMCard is inserted or removed
     */
    KbScan();
    CardCheckCard();
}


/* ����������������������������������������������������������������������� */
/*!
 * \brief Initialise Digital IO
 *  init inputs to '0', outputs to '1' (DDRxn='0' or '1')
 *
 *  Pull-ups are enabled when the pin is set to input (DDRxn='0') and then a '1'
 *  is written to the pin (PORTxn='1')
 */
/* ����������������������������������������������������������������������� */
void SysInitIO(void)
{
    /*
     *  Port B:     VS1011, MMC CS/WP, SPI
     *  output:     all, except b3 (SPI Master In)
     *  input:      SPI Master In
     *  pull-up:    none
     */
    outp(0xF7, DDRB);

    /*
     *  Port C:     Address bus
     */

    /*
     *  Port D:     LCD_data, Keypad Col 2 & Col 3, SDA & SCL (TWI)
     *  output:     Keyboard colums 2 & 3
     *  input:      LCD_data, SDA, SCL (TWI)
     *  pull-up:    LCD_data, SDA & SCL
     */
    outp(0x0C, DDRD);
    outp((inp(PORTD) & 0x0C) | 0xF3, PORTD);

    /*
     *  Port E:     CS Flash, VS1011 (DREQ), RTL8019, LCD BL/Enable, IR, USB Rx/Tx
     *  output:     CS Flash, LCD BL/Enable, USB Tx
     *  input:      VS1011 (DREQ), RTL8019, IR
     *  pull-up:    USB Rx
     */
    outp(0x8E, DDRE);
    outp((inp(PORTE) & 0x8E) | 0x01, PORTE);

    /*
     *  Port F:     Keyboard_Rows, JTAG-connector, LED, LCD RS/RW, MCC-detect
     *  output:     LCD RS/RW, LED
     *  input:      Keyboard_Rows, MCC-detect
     *  pull-up:    Keyboard_Rows, MCC-detect
     *  note:       Key row 0 & 1 are shared with JTAG TCK/TMS. Cannot be used concurrent
     */
#ifndef USE_JTAG
    sbi(JTAG_REG, JTD); // disable JTAG interface to be able to use all key-rows
    sbi(JTAG_REG, JTD); // do it 2 times - according to requirements ATMEGA128 datasheet: see page 256
#endif //USE_JTAG

    outp(0x0E, DDRF);
    outp((inp(PORTF) & 0x0E) | 0xF1, PORTF);

    /*
     *  Port G:     Keyboard_cols, Bus_control
     *  output:     Keyboard_cols
     *  input:      Bus Control (internal control)
     *  pull-up:    none
     */
    outp(0x18, DDRG);
}

/* ����������������������������������������������������������������������� */
/*!
 * \brief Starts or stops the 4.44 msec mainbeat of the system
 * \param OnOff indicates if the mainbeat needs to start or to stop
 */
/* ����������������������������������������������������������������������� */
static void SysControlMainBeat(u_char OnOff)
{
    int nError = 0;

    if (OnOff==ON)
    {
        nError = NutRegisterIrqHandler(&OVERFLOW_SIGNAL, SysMainBeatInterrupt, NULL);
        if (nError == 0)
        {
            init_8_bit_timer();
        }
    }
    else
    {
        // disable overflow interrupt
        disable_8_bit_timer_ovfl_int();
    }
}

/* ---------- end of module ------------------------------------------------ */

/*@}*/
