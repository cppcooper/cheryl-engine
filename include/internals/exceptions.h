#include <exception>

namespace CherylE
{
    class base_exception : public std::exception{
    protected:
        char msg[2048];
    public:
        base_exception(const char* function, uint32_t line, const char* info){
            snprintf(msg,2048,"[%s:%d]\n",function,line);
            size_t l = strlen(msg);
            strncpy(msg+l,info,2048-l-1);
        }

        virtual const char* what() const {
            return msg;
        }
    };

    class bad_alloc : public base_exception{
    public:
        char stacktrace[4096];
        bad_alloc(const char* location_, uint32_t line_)
         :base_exception(location_, line_,"bad allocation"){
            //todo: fill stacktrace
        }
    };
}

std::ostream& operator<<(std::ostream &os, CherylE::base_exception &obj){
    os << obj.what();
    return os;
}
