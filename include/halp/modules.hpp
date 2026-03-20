#pragma once

// clang-format off
#if defined(HALP_MODULE_BUILD)
#define HALP_MODULE_EXPORT export
#define HALP_MODULE_BEGIN_EXPORT export {
#define HALP_MODULE_END_EXPORT }
#else
#define HALP_MODULE_EXPORT 
#define HALP_MODULE_BEGIN_EXPORT 
#define HALP_MODULE_END_EXPORT 
#endif
// clang-format on
