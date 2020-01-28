//
// Created by jojos38 on 28.01.2020.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <ctime>
#include<sstream>
#include <string.h>

void setLoggerLevel(char level[]);
void info(char obj[]);
void error(char obj[]);
void warn(char obj[]);
void debug(char obj[]);

#endif // LOGGER_H
