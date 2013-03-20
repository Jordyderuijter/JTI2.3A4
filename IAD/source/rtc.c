/* ========================================================================
 * [PROJECT]    SIR
 * [MODULE]     Real Time Clock
 * [TITLE]      High- and low level Routines for INtersil X1205 RTC chip
 * [FILE]       rtc.c
 * [VSN]        1.0
 * [CREATED]    13042007
 * [LASTCHNGD]  131042007
 * [COPYRIGHT]  Copyright (C) STREAMIT BV 2010
 * [PURPOSE]    contains all interface- and low-level routines to
 *              read/write date/time/status strings from the X1205
 * ======================================================================== */

#define LOG_MODULE  LOG_RTC_MODULE

#include <cfg/os.h>
#include <dev/twif.h>
#include <sys/event.h>
#include <sys/timer.h>

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "rtc.h"
#include "portio.h"
#include "log.h"
#include "keyboard.h"
#include "vs10xx.h"
#include "display.h"
#include "flash.h"

#define I2C_SLA_RTC         0x6F
#define I2C_SLA_EEPROM      0x57
#define EEPROM_PAGE_SIZE    64


static u_long rtc_status;
int alarm_a_on = 0;
int alarm_b_on = 0;

/*!
 * \brief Enable or disable write access.
 *
 * \param on Write access is disabled if this parameter is 0, or
 *           enabled otherwise.
 *
 * \return 0 on success or -1 in case of an error.
 */
static int X12WriteEnable(int on)
{
    int rc;
    u_char buf[3];

    buf[0] = 0;
    buf[1] = 0x3F;
    if (on)
    {
        buf[2] = 0x02;
        if ((rc = TwMasterTransact(I2C_SLA_RTC, buf, 3, 0, 0, NUT_WAIT_INFINITE)) == 0)
        {
            buf[2] = 0x06;
            rc = TwMasterTransact(I2C_SLA_RTC, buf, 3, 0, 0, NUT_WAIT_INFINITE);
        }
    }
    else
    {
        buf[2] = 0x00;
        rc = TwMasterTransact(I2C_SLA_RTC, buf, 3, 0, 0, NUT_WAIT_INFINITE);
    }
    return(rc);
}

/*!
 * \brief Wait until non-volatile write cycle finished.
 *
 * \return 0 on success or -1 in case of an error.
 */
static int X12WaitReady(void)
{
    u_char poll;
    int cnt = 20;

    /* Poll for write cycle finished. */
    while (--cnt && TwMasterTransact(I2C_SLA_EEPROM, 0, 0, &poll, 1, NUT_WAIT_INFINITE) == -1)
    {
        NutSleep(1);
    }
    return(cnt ? 0 : -1);
}

/*!
 * \brief Read RTC registers.
 *
 * \param reg  The first register to read.
 * \param buff Pointer to a buffer that receives the register contents.
 * \param cnt  The number of registers to read.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcReadRegs(u_char reg, u_char *buff, size_t cnt)
{
    int rc = -1;
    u_char wbuf[2];

    wbuf[0] = 0;
    wbuf[1] = reg;
    if (TwMasterTransact(I2C_SLA_RTC, wbuf, 2, buff, cnt, NUT_WAIT_INFINITE) == cnt)
    {
        rc = 0;
    }
    return(rc);
}

/*!
 * \brief Write to RTC registers.
 *
 * \param nv   Must be set to 1 when writing to non-volatile registers.
 *             In this case the routine will poll for write cycle
 *             completion before returning to the caller. Set to zero
 *             if writing to volatile registers.
 * \param buff This buffer must contain all bytes to be transfered to
 *             the RTC chip, including the register address.
 * \param cnt  Number of valid bytes in the buffer.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcWrite(int nv, CONST u_char *buff, size_t cnt)
{
    int rc;

    if ((rc = X12WriteEnable(1)) == 0)
    {
        rc = TwMasterTransact(I2C_SLA_RTC, buff, cnt, 0, 0, NUT_WAIT_INFINITE);
        if (rc == 0 && nv)
        {
            rc = X12WaitReady();
        }
        X12WriteEnable(0);
    }
    return(rc);
}

/*!
 * \brief Get date and time from an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcGetTime().
 *
 * \param tm Points to a structure that receives the date and time
 *           information.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcGetClock(struct _tm *tm)
{
    int rc;
    u_char data[8];

    if ((rc = X12RtcReadRegs(X12RTC_SC, data, 8)) == 0)
    {
        tm->tm_sec = BCD2BIN(data[0]);
        tm->tm_min = BCD2BIN(data[1]);
        tm->tm_hour = BCD2BIN(data[2] & 0x3F);
        tm->tm_mday = BCD2BIN(data[3]);
        tm->tm_mon = BCD2BIN(data[4]) - 1;
        tm->tm_year = BCD2BIN(data[5]) + 100;
        if (BCD2BIN(data[7]) > 0x19)
        {
            tm->tm_year += 100;
        }
        tm->tm_wday = data[6];
    }
    return(rc);
}

/*!
 * \brief Set an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcSetTime().
 *
 * New time will be taken over at the beginning of the next second.
 *
 * \param tm Points to a structure which contains the date and time
 *           information.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcSetClock(CONST struct _tm *tm)
{
    u_char data[10];

    memset(data, 0, sizeof(data));
    if (tm)
    {
        data[1] = X12RTC_SC;
        data[2] = BIN2BCD(tm->tm_sec);
        data[3] = BIN2BCD(tm->tm_min);
        data[4] = BIN2BCD(tm->tm_hour) | 0x80;
        data[5] = BIN2BCD(tm->tm_mday);
        data[6] = BIN2BCD(tm->tm_mon + 1);
        if (tm->tm_year > 99)
        {
            data[7] = BIN2BCD(tm->tm_year - 100);
            data[9] = 0x20;
        }
        else
        {
            data[7] = BIN2BCD(tm->tm_year);
            data[9] = 0x19;
        }
        data[8] = tm->tm_wday;
    }
    return(X12RtcWrite(0, data, 10));
}

/*!
 * \brief Get alarm date and time of an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcGetAlarm().
 *
 * \param idx   Zero based index. Two alarms are supported.
 * \param tm    Points to a structure that receives the date and time
 *              information.
 * \param aflgs Points to an unsigned long that receives the enable flags.
 *
 * \return 0 on success or -1 in case of an error.
 *
 */
int X12RtcGetAlarm(int idx, struct _tm *tm, int *aflgs)
{
    int rc;
    u_char data[8];

    *aflgs = 0;
    memset(tm, 0, sizeof(struct _tm));
    if ((rc = X12RtcReadRegs(idx * 8, data, 8)) == 0)
    {
        if (data[0] & X12RTC_SCA_ESC)
        {
            *aflgs |= RTC_ALARM_SECOND;
            tm->tm_sec = BCD2BIN(data[0] & 0x7F);
        }
        if (data[1] & X12RTC_MNA_EMN)
        {
            *aflgs |= RTC_ALARM_MINUTE;
            data[1] &= 0b01111111;
            tm->tm_min = BCD2BIN(data[1]);
        }
        if (data[2] & X12RTC_HRA_EHR)
        {
            *aflgs |= RTC_ALARM_HOUR;
            tm->tm_hour = BCD2BIN(data[2] & ~0x80);
        }
        if (data[3] & X12RTC_DTA_EDT)
        {
            *aflgs |= RTC_ALARM_MDAY;
            tm->tm_mday = BCD2BIN(data[3]);
        }
        if (data[4] & X12RTC_MOA_EMO)
        {
            *aflgs |= RTC_ALARM_MONTH;
            tm->tm_mon = BCD2BIN(data[4]) - 1;
        }
        if (data[6] & X12RTC_DWA_EDW)
        {
            *aflgs |= RTC_ALARM_WDAY;
            tm->tm_wday = BCD2BIN(data[6]);
        }
    }
    return(rc);
}

/*!
 * \brief Set alarm of an X12xx hardware clock.
 *
 * \deprecated New applications must use NutRtcSetAlarm().
 *
 * \param idx   Zero based index. Two alarms are supported.
 * \param tm    Points to a structure which contains the date and time
 *              information. May be NULL to clear the alarm.
 * \param aflgs Each bit enables a specific comparision.
 *              - Bit 0: Seconds
 *              - Bit 1: Minutes
 *              - Bit 2: Hours
 *              - Bit 3: Day of month
 *              - Bit 4: Month
 *              - Bit 7: Day of week (Sunday is zero)
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcSetAlarm(int idx, CONST struct _tm *tm, int aflgs)
{
    u_char data[10];

    memset(data, 0, sizeof(data));
    data[1] = idx * 8;
    if (tm)
    {
        if (aflgs & RTC_ALARM_SECOND)
        {
            data[2] = BIN2BCD(tm->tm_sec) | X12RTC_SCA_ESC;
        }
        if (aflgs & RTC_ALARM_MINUTE)
        {
            data[3] = BIN2BCD(tm->tm_min) | X12RTC_MNA_EMN;
        }
        if (aflgs & RTC_ALARM_HOUR)
        {
            data[4] = BIN2BCD(tm->tm_hour) | X12RTC_HRA_EHR;
        }
        if (aflgs & RTC_ALARM_MDAY)
        {
            data[5] = BIN2BCD(tm->tm_mday) | X12RTC_DTA_EDT;
        }
        if (aflgs & RTC_ALARM_MONTH)
        {
            data[6] = BIN2BCD(tm->tm_mon + 1) | X12RTC_MOA_EMO;
        }
        if (aflgs & RTC_ALARM_WDAY)
        {
            data[8] = BIN2BCD(tm->tm_wday) | X12RTC_DWA_EDW;
        }
    }
    return(X12RtcWrite(1, data, 10));
}

/*!
 * \brief Query RTC status flags.
 *
 * \deprecated New applications must use NutRtcGetStatus().
 *
 * \param sflgs Points to an unsigned long that receives the status flags.
 *              - Bit 0: Power fail.
 *              - Bit 5: Alarm 0 occured.
 *              - Bit 6: Alarm 1 occured.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12RtcGetStatus(u_long *sflgs)
{
    int rc;
    u_char data;

    if ((rc = X12RtcReadRegs(X12RTC_SR, &data, 1)) == 0)
    {
        rtc_status |= data;
        *sflgs = rtc_status;
    }
    return(rtc_status);
}

/*!
 * \brief Clear RTC status flags.
 *
 * \deprecated New applications must use NutRtcClearStatus().
 *
 * \param sflgs Status flags to clear.
 *
 * \return Always 0.
 */
int X12RtcClearStatus(u_long sflgs)
{
    rtc_status &= ~sflgs;

    return(0);
}

/*!
 * \brief Read contents from non-volatile EEPROM.
 *
 * \param addr  Start location.
 * \param buff  Points to a buffer that receives the contents.
 * \param len   Number of bytes to read.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12EepromRead(u_int addr, void *buff, size_t len)
{
    int rc = -1;
    u_char wbuf[2];

    wbuf[0] = (u_char)(addr >> 8);
    wbuf[1] = (u_char)addr;
    if (TwMasterTransact(I2C_SLA_EEPROM, wbuf, 2, buff, len, NUT_WAIT_INFINITE) == len)
    {
        rc = 0;
    }
    return(rc);
}

/*!
 * \brief Store buffer contents in non-volatile EEPROM.
 *
 * The EEPROM of the X122x has a capacity of 512 bytes, while the X1286 is
 * able to store 32 kBytes.
 *
 * \param addr  Storage start location.
 * \param buff  Points to a buffer that contains the bytes to store.
 * \param len   Number of valid bytes in the buffer.
 *
 * \return 0 on success or -1 in case of an error.
 */
int X12EepromWrite(u_int addr, CONST void *buff, size_t len)
{
    int rc = 0;
    u_char *wbuf;
    size_t wlen;
    CONST u_char *wp = buff;

    /*
     * Loop for each page to be written to.
     */
    while (len)
    {
        /* Do not cross page boundaries. */
        wlen = EEPROM_PAGE_SIZE - (addr & (EEPROM_PAGE_SIZE - 1));
        if (wlen > len)
        {
            wlen = len;
        }

        /* Allocate and set a TWI write buffer. */
        if ((wbuf = malloc(wlen + 2)) == 0)
        {
            rc = -1;
            break;
        }
        wbuf[0] = (u_char)(addr >> 8);
        wbuf[1] = (u_char)addr;
        memcpy(wbuf + 2, (void *)wp, wlen);

        /* Enable EEPROM write access and send the write buffer. */
        if ((rc = X12WriteEnable(1)) == 0)
        {
            rc = TwMasterTransact(I2C_SLA_EEPROM, wbuf, wlen + 2, 0, 0, NUT_WAIT_INFINITE);
        }

        /* Release the buffer and check the result. */
        free(wbuf);
        if (rc)
        {
            break;
        }
        len -= wlen;
        addr += wlen;
        wp += wlen;

        /* Poll for write cycle finished. */
        if ((rc = X12WaitReady()) != 0)
        {
            break;
        }
    }
    X12WriteEnable(0);

    return(rc);
}

/*!
 * \brief Initialize the interface to an Intersil X12xx hardware clock.
 *
 * \deprecated New applications must use NutRegisterRtc().
 *
 * \return 0 on success or -1 in case of an error.
 *
 */
int X12Init(void)
{
    int rc;
    u_long tmp;

    if ((rc = TwInit(0)) == 0)
    {
        rc = X12RtcGetStatus(&tmp);
    }
    return (rc);
}

/**
 * 
 * @param time, sets alarm A on time, in hours and minutes
 */
void set_alarm_a(tm* time)
{
    X12RtcSetAlarm(0, time, 0b00000110);
    alarm_a_on = 1;
    alarmstatus_changed = true;
}

/**
 * 
 * @param gmt
 * @return returns time set for alarm A
 */
void get_alarm_a(tm* gmt)
{
    int flags = 0;
    X12RtcGetAlarm(0, gmt, &flags);
}

/**
 * 
 * @param time, sets alarm B on time, in years, months, days, hours and minutes
 */
void set_alarm_b(tm* time)
{
    X12RtcSetAlarm(1, time, 0b00011110);
    alarm_b_on = 1;
    alarmstatus_changed = true;
}

/**
 * 
 * @param gmt
 * @return returns time set for alarm B
 */
void get_alarm_b(tm* gmt)
{
    int flags = 0;
    X12RtcGetAlarm(1, gmt, &flags);
}

void disable_alarm_b()
{
    tm dummy;
    X12RtcSetAlarm(1, &dummy, 0b00000000);
}

void get_snooze(short* gmt)
{
    At45dbPageRead(2, gmt, 2);
}

THREAD(AlarmPollingThread, arg)
{
    static u_int button_cooldown_counter = 0;
    static bool button_cooldown = true;
    u_long alarm_a;
    u_long alarm_b; 
    short snooze_time;
    tm snooze;
    tm alarm_a_backup;
    
    for(;;)
    {      
        X12RtcGetStatus(&alarm_a);
        alarm_b = alarm_a;
        alarm_a &= 0b00100000;

        alarm_b &= 0b01000000;

        if(alarm_a!=0 && alarm_a_on)
        {
            VsBeep((u_char)500, 1000);              //BeepBoop, beeps with (frequency, duration)
            VsSetVolume(0,0);
        }

        if(alarm_b!=0 && alarm_b_on)
        {
            VsBeep((u_char)1000, 1000);              //BeepBoop, beeps with (frequency, duration)
            VsSetVolume(0,0);
        }

        if(button_cooldown_counter >= 2)
        {
                button_cooldown = false;
                button_cooldown_counter = 0;
        }
    
        if(kb_button_is_pressed(KEY_ALT) && alarm_a!=0)
        {
            At45dbPageRead(2, &snooze_time, 2);
            X12RtcClearStatus(0b00100000);
            get_alarm_a(&snooze);
            snooze.tm_min+=snooze_time;
            set_alarm_a(&snooze);
        }
        
        if(!button_cooldown)
        {
            switch(kb_button_pressed())
            {
                case KEY_ESC:
                    if(VsGetVolume() > 0)
                    {
                        VsSetVolume(0, 0);
                        puts("volume 0");
                    }
                    else 
                    {
                        VsSetVolume(100, 100);
                        puts("volume 0");
                    } 
                    if(alarm_a != 0)
                    {
                        X12RtcClearStatus(0b00100000);
                        At45dbPageRead(3, &alarm_a_backup, sizeof(tm));
                        set_alarm_a(&alarm_a_backup);
                    }
                    if(alarm_b != 0)
                    {
                        X12RtcClearStatus(0b01000000);
                        disable_alarm_b();
                        alarm_b_on = false;
                    }
                    break;

                case KEY_01:
                    alarm_a_on = !alarm_a_on;
                    alarmstatus_changed = true;
                    break;

                case KEY_02:
                    alarm_b_on = !alarm_b_on;
                    alarmstatus_changed = true;
                    break;

                default:
                    break;
            }
            button_cooldown = true;
        }
        if(button_cooldown)
                button_cooldown_counter++; 
        NutSleep(100);
    }
}

/**
* Adjusts the given timestamp to the given timezone.
* @param timestamp The timestamp to adjust
* @param utc_offset The timezone adjustment to apply
*/
void rtc_get_timezone_adjusted_timestamp(tm* timestamp, tm* utc_offset)
{
    time_t current_time;
    
    //Initialization (? perhaps not necessary ?)
    _timezone = 0;    
    current_time = mktime(timestamp);

    // Set new time
    _timezone = -(long)((utc_offset->tm_hour  * 60L * 60L) + (utc_offset->tm_min * 60L)); // This will be used in time.h functions.
    *timestamp = *localtime(&current_time);
    _timezone = 0; // 'Reset' timezone. In this particular application not necessary and probably even efficient, but let's do it anyway to avoid possible confusion.
    
}

