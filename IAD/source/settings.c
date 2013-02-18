
tm alarm_a;
tm alarm_b;
tm current_time;
int snoozetime_a;

//Set time for alarm A. This alarm will go off every day.
void settings_set_alarm_a(tm time)
{
    alarm_a = time;
}

//Set time and date for alarm B.
void settings__set_alarm_b(tm time)
{
    alarm_b = time;
}

//Set snooze time in minutes. (max 120)
void settings_set_snoozetime_a(int minutes)
{
    if(minutes <= 120)
        snoozetime_a = minutes;
}

void settings_set_current_time(tm time)
{
    current_time = time;
}