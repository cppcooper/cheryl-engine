#include <backward.hpp>

backward::SignalHandling sh;

int main() {
    backward::TraceResolver tr;
    backward::StackTrace st;
    try {
        *(int*)0 = 0;
    } catch (...) {
        return 0;
    }
    return 0;
}