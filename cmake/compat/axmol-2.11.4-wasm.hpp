#pragma once

// Axmol 2.11.4's ParticleSystem.cpp calls htonl without including its
// declaration. Emscripten 3.1.73 intentionally exposes it through this POSIX
// header, so force-including this shim keeps the verified upstream tree clean.
#if defined(__EMSCRIPTEN__)
#  include <arpa/inet.h>
#endif
