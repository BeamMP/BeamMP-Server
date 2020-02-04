//
// Created by Anonymous275 on 4/2/2020.
//

#define LOGGER_H

#include <iostream>
#include <ctime>
#include <sstream>
#include <string.h>
using namespace std;
extern int loggerlevel;
stringstream getDate();
void setLoggerLevel(char level_string[]);
void info(basic_string<char> toPrint);
void warn(basic_string<char> toPrint);
void error(basic_string<char> toPrint);
void debug(basic_string<char> toPrint);
