//
// Created by jojos38 on 28.01.2020.
//

#include "logger.h"
using namespace std;

int level = 0;

void setLoggerLevel(char level_string[]) {
    if (!strcmp(level_string, "ALL"))
        level = 0;

    if (!strcmp(level_string, "DEBUG"))
        level = 1;

    if (!strcmp(level_string, "INFO"))
        level = 2;

    if (!strcmp(level_string, "WARN"))
        level = 3;

    if (!strcmp(level_string, "ERROR"))
        level = 4;

    if (!strcmp(level_string, "OFF"))
        level = 5;
}

stringstream getDate() {
    // current date/time based on current system
    time_t now = time(0);
    tm* ltm = localtime(&now);
    
    int month = 1 + ltm->tm_mon;
    int day = ltm->tm_mday;
    int hours = 1 + ltm->tm_hour;
    int minutes = 1 + ltm->tm_min;
    int seconds = 1 + ltm->tm_sec;

    string month_string;
    if (month < 10) month_string = "0" + to_string(month);
    else month_string = to_string(month);

    string day_string;
    if (day < 10) day_string = "0" + to_string(day);
    else day_string = to_string(day);

    string hours_string;
    if (hours < 10) hours_string = "0" + to_string(hours);
    else hours_string = to_string(hours);

    string minutes_string;
    if (minutes < 10) minutes_string = "0" + to_string(minutes);
    else minutes_string = to_string(minutes);

    string seconds_string;
    if (seconds < 10) seconds_string = "0" + to_string(seconds);
    else seconds_string = to_string(seconds);

    std::stringstream date;
    date
        << "["
        << day_string << "/"
        << month_string << "/"
        << 1900 + ltm->tm_year << " "
        << hours_string << ":"
        << minutes_string << ":"
        << seconds_string
        << "] ";
    return date;
}

void info(char obj[]) {
    if (level <= 2)
        cout << getDate().str() << "\u001b[36m" << "[INFO]" << "\u001b[0m" << " " << obj << endl;
}

void error(char obj[]) {
    if (level <= 4)
        cout << getDate().str() << "\x1B[31m" << "[ERRO]" << "\u001b[0m" << " " << obj << endl;
}

void warn(char obj[]) {
    if (level <= 3)
        cout << getDate().str() << "\u001b[33m" << "[WARN]" << "\u001b[0m" << " " << obj << endl;
}

void debug(char obj[]) {
    if (level <= 1)
        cout << getDate().str() << "\u001b[35m" << "[DBUG]" << "\u001b[0m" << " " << obj << endl;
}