#pragma once

// one of BEAMMP_{WINDOWS,LINUX,APPLE} will be set at the end of this

// clang-format off
#if !defined(BEAMMP_WINDOWS) && !defined(BEAMMP_UNIX) && !defined(BEAMMP_APPLE) && !defined(BEAMMP_FREEBSD)
    #if defined(_WIN32) || defined(__CYGWIN__)
        #define BEAMMP_WINDOWS
    #elif defined(__linux__) || defined(__linux) || defined(linux)
        #define BEAMMP_LINUX
    #elif defined(__FreeBSD__)
	#define BEAMMP_FREEBSD
    #elif defined(__APPLE__) || defined(__MACH__)
        #define BEAMMP_APPLE
    #else
        #error "This platform is not known. Please define one of the above for your OS."
    #endif
#endif
// clang-format on
