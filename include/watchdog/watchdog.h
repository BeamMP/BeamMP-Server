//
// Created by Anonymous275 on 9/9/2021.
//

#pragma once
#include <cstdint>
extern void watchdog_init(const char* crashFile, const char* SpecificPDBLocation, bool Symbols = true);
extern void generate_crash_report(uint32_t Code, void* Address);
const char* getFunctionDetails(void* Address);
extern void watchdog_setOffset(int64_t Off);
const char* getCrashLocation(void* Address);
void InitSym(const char* PDBLocation);