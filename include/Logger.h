///
/// Created by Anonymous275 on 4/2/2020.
///
#pragma once
#include <string>
#include <iostream>
void InitLog();
void except(const std::string& toPrint);
void debug(const std::string& toPrint);
void error(const std::string& toPrint);
void info(const std::string& toPrint);
void warn(const std::string& toPrint);
