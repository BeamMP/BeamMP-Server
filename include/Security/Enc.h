// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/28/2020
///
#pragma once
#ifdef __linux
#define EXCEPTION_POINTERS void
#else
#include <WS2tcpip.h>
#endif
#include <string>
int Handle(EXCEPTION_POINTERS* ep, char* Origin);
