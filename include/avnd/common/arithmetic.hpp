#pragma once

#define avnd_for_all_integer_types(f) \
f(long long) \
    f(long) \
    f(int) \
    f(short) \
    f(char) \
    f(unsigned long long) \
    f(unsigned long) \
    f(unsigned int) \
    f(unsigned short) \
    f(unsigned char) \
    f(double) \
    f(float)

#define avnd_for_all_fp_types(f) \
    f(double) \
    f(float)  \
    f(long long) \
    f(long) \
    f(int) \
    f(short) \
    f(char) \
    f(unsigned long long) \
    f(unsigned long) \
    f(unsigned int) \
    f(unsigned short) \
    f(unsigned char)
