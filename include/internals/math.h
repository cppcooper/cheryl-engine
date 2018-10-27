#pragma once
#ifndef MATH_H
#define MATH_H

long long nextPowerof2(long long v){
    int bits = sizeof(long long)*8;
    v--;
    for(int i = 1; i < bits; i <<= 1){
        v |= v >> i;
    }
    return ++v;
}

#endif