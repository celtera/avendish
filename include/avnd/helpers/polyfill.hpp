#pragma once
#if __clang_major__ <= 13
#define clang_buggy_consteval constexpr
#else
#define clang_buggy_consteval consteval
#endif
