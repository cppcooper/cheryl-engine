#include <core/logging/log.h>
#include <backward.hpp>
#include <spanstream>

namespace CE {
    uint16_t log_counter = 0;

    std::string stack_trace(void *addr0) {
        static thread_local std::array<char, 6144> buffer{};
        static thread_local std::span bspan(buffer);
        static thread_local std::spanstream ss(bspan);
        //inform ss this is the end of any strings, required for repeat executions
        buffer[0] = '\0';
        //need to reset the position cursor
        ss.seekp(0);
        // We need to start generating the stack trace now.
        using namespace backward;
        StackTrace st;
        // ideally we're going to shorten the stack trace to near addr0
        if(addr0) {
            st.load_from(addr0, 7);
        } else {
            st.load_here(17);
        }
        TraceResolver tr; tr.load_stacktrace(st);
        // manually prepare stack trace
        for (size_t i = 0; i < st.size(); ++i) {
            backward::ResolvedTrace trace = tr.resolve(st[i]);
            ss<<"#"<<i
              <<" "<<trace.object_function
              <<"["<<trace.addr<<"]"
              <<" in "<<trace.source.filename
              <<":"<<trace.source.line<<":"<<trace.source.col
              <<std::endl;
        }
        ss<<'\0'; //null terminate our string, required for repeat executions
        return buffer.data();
    }

}
