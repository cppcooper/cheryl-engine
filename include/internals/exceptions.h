#include <exception>

namespace CherylE
{
    class base_exception : public std::exception{
    protected:
        char msg[2048];
        char stacktrace[4096];
    public:
        base_exception(const char* function, uint32_t line, const char* info){
            snprintf(msg,2048,"[%s:%d]\n",function,line);
            size_t l = strlen(msg);
            strncpy(msg+l,info,2048-l-1);
            //todo: fill stacktrace
        }

        virtual const char* what() const {
            static char buffer[2048+4096];
            //todo: sprintf into buffer
            return msg;
        }
    };

    class bad_alloc : public base_exception{
    public:
        bad_alloc(const char* location_, uint32_t line_)
         :base_exception(location_, line_,"bad allocation"){}
    };

    class invalid_args : public base_exception{
    public:
        invalid_args(const char* location_, uint32_t line_)
            : base_exception(location_, line_,"invalid args"){}
        invalid_args(const char* location_, uint32_t line_, const char* info_)
            : base_exception(location_, line_, info_){}
    }
}

std::ostream& operator<<(std::ostream &os, CherylE::base_exception &obj){
    os << obj.what();
    return os;
}
