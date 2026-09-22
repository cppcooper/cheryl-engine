#pragma once
#include <string>
#include <format>

inline std::string human_readable(const std::size_t bytes) {
    std::stringstream ss;
    double XiB = bytes;
    uint8_t counter = 0;
    while(XiB > 1024) {
        XiB /= 1024;
        counter++;
    }
    std::string suffix;
    switch(counter) {
        case 0:
            suffix = " bytes";
            break;
        case 1:
            suffix = "KiB";
            break;
        case 2:
            suffix = "MiB";
            break;
        case 3:
            suffix = "GiB";
            break;
        case 4:
            suffix = "PiB";
            break;
        case 5:
            suffix = "EiB";
            break;
        case 6:
            suffix = "ZiB";
            break;
        case 7:
            suffix = "YiB";
            break;
        default:
            break;
    }
    return std::format("{:3.1f}{}",XiB,suffix);
}
