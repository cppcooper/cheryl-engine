#pragma once
#ifndef CELOADABLE_H
#define CELOADABLE_H
#include <fstream>

class loadable{
public:
    virtual void load(const char*) = 0;
    virtual void load(const loadable&) = 0;
    virtual void load(std::fstream&) = 0;
};

#endif