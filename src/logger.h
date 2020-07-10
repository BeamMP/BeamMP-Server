///
/// Created by Anonymous275 on 4/2/2020.
///

#include <ctime>
#include <sstream>
#include <iostream>

extern int loggerlevel;
std::stringstream getDate();
void setLoggerLevel(int level);
void info(const std::string& toPrint);
void warn(const std::string& toPrint);
void error(const std::string& toPrint);
void debug(const std::string& toPrint);
void Exception(unsigned long Code,char* Origin);