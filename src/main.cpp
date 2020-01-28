//
// Created by Антон on 28.01.2020.
//

#include "main.h"
#include "network.h"
#include "logger.h"

int main() {
	// ALL > DEBUG > INFO > WARN > ERROR > OFF
	setLoggerLevel("ALL");
	startRUDP("localhost", 30814);
}
