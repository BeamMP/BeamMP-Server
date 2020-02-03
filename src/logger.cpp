//
// Created by jojos38 on 28.01.2020.
//


#include <fstream>
#include "logger.h"
void addToLog(basic_string<char> Data);
using namespace std;
int loggerlevel;

void setLoggerLevel(char level_string[]) {
    if (!strcmp(level_string, "ALL"))
        loggerlevel = 0;

    if (!strcmp(level_string, "DEBUG"))
        loggerlevel = 1;

    if (!strcmp(level_string, "INFO"))
        loggerlevel = 2;

    if (!strcmp(level_string, "WARN"))
        loggerlevel = 3;

    if (!strcmp(level_string, "ERROR"))
        loggerlevel = 4;

    if (!strcmp(level_string, "OFF"))
        loggerlevel = 5;
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









void info(basic_string<char> toPrint) {
    if (loggerlevel <= 2){
        cout << getDate().str() << "[INFO] " << toPrint << endl;
        addToLog(getDate().str() + "[INFO] " + toPrint + "\n");
    }
}

void error(basic_string<char> toPrint) {
    if (loggerlevel <= 4) {
        cout << getDate().str() << "[ERROR] " << toPrint << endl;
        addToLog(getDate().str() + "[ERROR] " + toPrint + "\n");
    }
}


void warn(basic_string<char> toPrint) {
    if (loggerlevel <= 3) {
        cout << getDate().str() << "[WARN] " << toPrint << endl;
        addToLog(getDate().str() + "[WARN] " + toPrint + "\n");
    }
}


void debug(basic_string<char> toPrint) {
    if (loggerlevel <= 1) {
        cout << getDate().str() << "[DEBUG] " << toPrint << endl;
        addToLog(getDate().str() + "[DEBUG] " + toPrint + "\n");
    }
}