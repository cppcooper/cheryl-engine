#pragma once
#ifndef STACKTRACE_H
#define STACKTRACE_H

#include <vector>
#include <string>

//todo:make format compile time information?
class stacktracer{
private:
    std::vector<stackframe> stack;
    std::string             stacktrace;
    st_formatter            formatter;
protected:
//todo: use pure virtual methods for formatting?
    void AddFileName();
    void AddLineNumber();
    void AddSymbolName();
    void AddStackAddress();
    void AddBinaryName();
public:
    stacktracer();
    void print_stacktrace(ostream& out);
    void print_stacktrace(FILE* out);
    std::string get_stacktrace(uint8_t skip = 0);
};

#endif
