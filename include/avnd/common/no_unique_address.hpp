#pragma once

#if defined(_MSC_VER)
#define AVND_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define AVND_NO_UNIQUE_ADDRESS AVND_NO_UNIQUE_ADDRESS
#endif
