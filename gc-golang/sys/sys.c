//
// Created by root on 2021/4/17.
//
#include "gc.h"
#include "gpm.h"


byte deBruijnIdx64[64] = {
        0, 1, 2, 7, 3, 13, 8, 19,
        4, 25, 14, 28, 9, 34, 20, 40,
        5, 17, 26, 38, 15, 46, 29, 48,
        10, 31, 35, 54, 21, 50, 41, 57,
        63, 6, 12, 18, 24, 27, 33, 39,
        16, 37, 45, 47, 30, 53, 49, 56,
        62, 11, 23, 32, 36, 44, 52, 55,
        61, 22, 43, 51, 60, 42, 59, 58,
};

uint32 gcBlackenEnabled;
uint32 gcphase;
int32  ncpu;

m*     allm[10];


// inline function
uintptr round(uintptr n,uintptr a)
{
    return (n + a - 1) &~ (a - 1);
}
uint32 fastrand()
{
    m* mp = getg()->m;
    uint32  s1,s0;
    s1 = mp->fastrand[0];
    s0 = mp->fastrand[1];
    s1 ^= s1 << 17;
    s1 = s1 ^ s0 ^ s1>>7 ^ s0>>16;
    mp->fastrand[0] = s0;
    mp->fastrand[1] = s1;
    return s0 + s1;
}
int ctz64(uint64 x)
{
    x &= -x;                      // isolate low-order bit
    int y  = x * deBruijn64 >> 58;    // extract part of deBruijn sequence
    int i  = (int)(deBruijnIdx64[y]);   // convert to bit index
    int z  = (int)((x - 1) >> 57 & 64); // adjustment if zero
    return i + z;
}

