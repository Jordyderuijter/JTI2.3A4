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
void _handle_mainscreen_input(void);
void _handle_settings_input(void);
void _handle_timezone_setup_input(void);

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
    
    // If this is the first startup EVER, show the timezone setup.
    At45dbPageRead(0, &is_first_startup, 1);
    
    if(is_first_startup)
    {
        input_mode = 2;
        is_first_startup = false;
        At45dbPageWrite(0, &is_first_startup, 1);
    }
	
    for (;;)
    {
        // If a key is pressed, light up the LCD screen.
        if((kb_get_buttons_pressed_raw() ^ 0xFFFF) != 0)
            lcd_backlight_on(6);

        // Handle input based on the current input mode.
        switch(input_mode)
        {
            case 0:
                _handle_mainscreen_input();
                break;
            case 1:
                _handle_settings_input();
                break;
            case 2:
                _handle_timezone_setup_input();
                break;
        }
        
        // Mandatory main loop code.
        WatchDogRestart();
        NutSleep(100); 
    }

    return(0);      // never reached, but 'main()' returns a non-void, so.....
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
    SysControlMainBeat(ON);             // enable 4.4 msecs heartbeat interrupt

    /*
     * Increase our priority so we can feed the watchdog.
     */
    NutThreadSetPriority(1);

    /* Enable global interrupts */
    sei();
    
    //connect_to_internet();
    //ptime = get_ntp_time(0.0);
    //LogMsg_P(LOG_INFO, PSTR("NTP time [%02d:%02d:%02d]"), ptime->tm_hour, ptime->tm_min, ptime->tm_sec );
    
    NutThreadCreate("DisplayThread", DisplayThread, NULL, 512);                 // Start thread that handles the displaying on the LCD.
    // NutThreadCreate("AlarmPollingThread", AlarmPollingThread, NULL, 512);    // Start thread that constantly 'polls' for activated alarms.
    NutThreadCreate("InformationThread", InformationThread, NULL, 512);         // Start thread that handles the information display on the LCD.
}

/**
 *  Handles input in 'mainscreen mode'
 */
void _handle_mainscreen_input()
{
    if(kb_button_is_pressed(KEY_ESC) && kb_button_is_pressed(KEY_POWER))        // Go to timezone setup
    {
        lcd_display_string_at("Timezone: ", 0, 0);
        lcd_show_cursor(true);
        input_mode = 2;
    }
    else if(kb_button_is_pressed(KEY_OK))               // Go to settings menu
    {
        //lcd_display_settings_menu();
        input_mode = 1;
    }
    //else if(kb_button_is_pressed(KEY_UP))             // Display previous information item
        //Display previous information item here.
    //else if(kb_button_is_pressed(KEY_DOWN))           // Display next information item
        // Display next information item here.
}

/**
 * Handles input in 'settings menu mode'.
 */
void _handle_settings_input()
{
    // USE "MENU_SETTINGS_HANDLE_INPUT(key)" or something similar??
    
    // Handle settings menu input
        if(kb_button_is_pressed(KEY_UP))
            menu_settings_previous_item();
        else if(kb_button_is_pressed(KEY_DOWN))
            menu_settings_next_item();
        //else if(menu_get_current_menu_item() == 0 && kb_button_is_pressed(KEY_01))         // This can probably be done more efficient.
        // Change value of alarm A time.
}

/**
 * Handles input in 'timezone setup mode';
 */
void _handle_timezone_setup_input()
{
    static u_short time_cursor_position = 0;
    static u_short utc_offset = 0;

    if(kb_button_is_pressed(KEY_OK))            // Accept the current timezone offset and leave the setup screen.
    {
        tm timestamp;
        X12RtcGetClock(&timestamp);
        //_correct_timestamp_with_timezone(&timestamp, utc_offset);     Can this be done trough NTP??
        timestamp.tm_hour += utc_offset;
        X12RtcSetClock(&timestamp);
        
        lcd_show_cursor(false);
        input_mode = 0; // Switch input mode to mainscreen mode.
        
        // Clear display/show mainscreen here!?
        
        return;         // Since we have switched input mode we don't need to execute the other code in the function.
    }
    
    if(kb_button_is_pressed(KEY_LEFT))
    {
        if(time_cursor_position - 1 == -1)      // Use this check, we're using an UNSIGNED short here.
           time_cursor_position = 3;
        else
                time_cursor_position--;

        //lcd_place_cursor_at(/*Previous Position*/, 0);
    }
    else if(kb_button_is_pressed(KEY_RIGHT))
    {
        time_cursor_position++;
        
        if(time_cursor_position == 4)
            time_cursor_position = 0;
        
        //lcd_place_cursor_at(/*Next Position*/, 0);
    }
    
    if(time_cursor_position == 0)               // Position of 'ten-hours'
    {
        if(kb_button_is_pressed(KEY_01))
            utc_offset = 10 + utc_offset % 10;
        else if(kb_button_is_pressed(KEY_02))
                utc_offset = 2 + utc_offset % 10;
    }
    else if(time_cursor_position == 1)
    {
        if(kb_button_is_pressed(KEY_01))
        {
            utc_offset = (((u_short)utc_offset / 10) * 10) + 1;
            
            if(kb_button_is_pressed(KEY_ALT))
                    utc_offset += 5;
        }
        else if(kb_button_is_pressed(KEY_02))
        {
            utc_offset = (((u_short)utc_offset / 10) * 10) + 2;
            
            if(kb_button_is_pressed(KEY_ALT))
                utc_offset += 5;
        }
        else if(kb_button_is_pressed(KEY_03))
        {
            utc_offset = (((u_short)utc_offset / 10) * 10) + 3;
            
            if(kb_button_is_pressed(KEY_ALT))
                utc_offset += 5;
        }
         else if(kb_button_is_pressed(KEY_04))
         {
            utc_offset = (((u_short)utc_offset / 10) * 10) + 4;
            if(kb_button_is_pressed(KEY_ALT))
                utc_offset += 5;
         }
        else if(kb_button_is_pressed(KEY_05))
        {
            utc_offset = (((u_short)utc_offset / 10) * 10) + 5;
         
        if(kb_button_is_pressed(KEY_ALT))
            utc_offset = (((u_short)utc_offset / 10) * 10);
        }
    }
     
     // Display value here.
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

/* ---------- end of module ------------------------------------------------ */

/*@}*/
