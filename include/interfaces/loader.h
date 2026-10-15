#pragma once
#ifndef CELOADER_H
#define CELOADER_H
#include <fstream>

class Loader{
public:
    virtual loadable* load(const char*) = 0;
    virtual loadable* load(const std::string&) = 0;
    virtual loadable* load(std::fstream&) = 0;
};

#endif