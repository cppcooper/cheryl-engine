#include <iostream>
#include <exception>
#include <cinttypes>
#include <string>
#include <cstring>

namespace CherylE
{
    // single threaded function to construct a c-string for exception messages. UB if used on multiple threads concurrently
    template<typename... Args>
    const char* exception_msg(const char* format, Args... args) {
        static char buffer[2048] = {};
        snprintf(buffer, 2047, format, args...);
        return buffer;
    }

    class base_exception : public std::exception{
    protected:
        char msg[2048]{};
        char stacktrace[4096]{};
    public:
        base_exception(const char* exception_type, const char* function, uint32_t line, const char* info){
            snprintf(msg,2048,"exception: %s\n[%s:%d]\n",exception_type,function,line);
            size_t l = strlen(msg);
            strncpy(msg+l,info,2048-l-1);
            //todo: fill stacktrace
        }

        [[nodiscard("exception's what() is unused")]]
        const char* what() const noexcept override {
            static char buffer[2048+4096];
            //todo: sprintf into buffer
            return msg;
        }
    };

    class bad_alloc : public base_exception{
    public:
        bad_alloc(const char* location_, uint32_t line_)
         :base_exception("bad_alloc", location_, line_,"bad allocation"){}
    };

    class bad_request : public base_exception{
    public:
        bad_request(const char* location_, uint32_t line_, const char* info_)
         :base_exception("bad_request", location_, line_, info_){}
    };    

    class invalid_args : public base_exception{
    public:
        invalid_args(const char* location_, uint32_t line_)
            : base_exception("invalid_args", location_, line_,"invalid args"){}
        invalid_args(const char* location_, uint32_t line_, const char* info_)
            : base_exception("invalid_args", location_, line_, info_){}
    };

    class failed_operation : public base_exception{
    public:
        failed_operation(const char* location_, uint32_t line_, const char* info_)
            : base_exception("failed_operation", location_, line_, info_){}
    };
}

std::ostream& operator<<(std::ostream &os, CherylE::base_exception &obj){
    os << obj.what();
    return os;
}
