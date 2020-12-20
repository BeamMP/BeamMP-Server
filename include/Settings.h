// Copyright (c) 2020 Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/28/2020
///
#pragma once
#include <string>
extern std::string ServerName;
extern std::string ServerDesc;
extern std::string StatReport;
extern std::string FileSizes;
extern std::string Resource;
extern std::string FileList;
extern std::string CustomIP;
extern std::string MapName;
extern uint64_t MaxModSize;
extern std::string Key;
std::string GetSVer();
std::string GetCVer();
extern int MaxPlayers;
extern int ModsLoaded;
extern bool Private;
extern int MaxCars;
extern bool Debug;
extern int Port;
extern int PPS;