//
// Created by jojos38 on 28.01.2020.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <ctime>
#include <sstream>
#include <string.h>
using namespace std;

extern int loggerlevel;
stringstream getDate();
void setLoggerLevel(char level_string[]);

template<typename T>
void info(const T& toPrint) {
	if (loggerlevel <= 2)
		cout << getDate().str() << "[INFO] " << toPrint << endl;
}
template<typename T>
void error(const T& toPrint) {
    if (loggerlevel <= 4)
        cout << getDate().str() "[ERRO] " << toPrint << endl;
}

template<typename T>
void warn(const T& toPrint) {
    if (loggerlevel <= 3)
        cout << getDate().str() << "[WARN] " << toPrint << endl;
}

template<typename T>
void debug(const T& toPrint) {
    if (loggerlevel <= 1)
        cout << getDate().str() << "[DBUG] " << toPrint << endl;
}

#endif // LOGGER_H
