// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/28/2020
///
#include "Security/Enc.h"
#include "CustomAssert.h"
#include "Logger.h"
#include <sstream>
#include <thread>


#ifdef WIN32
int Handle(EXCEPTION_POINTERS* ep, char* Origin) {
    Assert(false);
    std::stringstream R;
    R << ("Code : ") << std::hex
      << ep->ExceptionRecord->ExceptionCode
      << std::dec << (" Origin : ") << Origin;
    except(R.str());
    return 1;
}
#else
// stub
int Handle(EXCEPTION_POINTERS*, char*) { return 1; }
#endif // WIN32
