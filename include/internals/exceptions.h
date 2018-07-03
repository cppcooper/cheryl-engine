#include <exception>

namespace CherylE
{
    class base_exception : std::exception
    {
    private:
        //todo: keep these?
        const char* msg;
    public:
        //todo: generate stack trace
        base_exception(const char*);
    };
}