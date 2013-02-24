#include <dev/board.h>
#include <sys/timer.h>
#include <sys/confnet.h>
 
#include <stdio.h>
#include <io.h>
#include <arpa/inet.h>
#include <pro/dhcp.h>
 
int main(void)
{
    connectToInternet();
 
    for (;;) {
        NutSleep(1000);
        getTime();
    }
    
    return 0;
}

void connectToInternet(void)
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

void getTime(void)
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
    printf("NTP time is: %02d:%02d:%02d\n", ntp_datetime->tm_hour, ntp_datetime->tm_min, ntp_datetime->tm_sec);
};