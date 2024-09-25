#include <internals/exceptions.h>
#include <backward.hpp>
#include <format>
#include <array>
#include <span>
#include <spanstream>

std::string insert_st() noexcept {
    static thread_local std::array<char, 2048> buffer{};
    static thread_local std::span<char, 2048> bspan(buffer);
    static thread_local std::spanstream ss(bspan);
    buffer[0] = '\0'; //inform ss this is the end of any strings, required for repeat executions
    ss.seekp(0); //need to reset the position cursor
    // trace the stack
    backward::StackTrace st;
    st.load_here(12);
    backward::TraceResolver tr;
    tr.load_stacktrace(st);
    // manually prepare stack trace
    for (size_t i = 4; i < st.size(); ++i) {
        backward::ResolvedTrace trace = tr.resolve(st[i]);
        ss<<"#"<<i-4
          <<" "<<trace.object_function
          <<"["<<trace.addr<<"]"
          <<" in "<<trace.source.filename
          <<":"<<trace.source.line<<":"<<trace.source.col
          <<std::endl;
    }
    ss<<'\0'; //null terminate our string, required for repeat executions
    //backward::Printer p;
    //p.object = true;
    //p.address = true;
    //p.print(st, ss);
    return buffer.data();
}

template<typename... Args>
CE::Exceptions::exception_base FormatException(const std::format_string<Args...> fmt, const Args&... args) noexcept {
    return CE::Exceptions::exception_base(std::vformat(fmt.get(), std::make_format_args(args...)));
}

CE::Exceptions::invalid_args::invalid_args(const char* location_, uint32_t line_) noexcept
        : CE::Exceptions::exception_base(FormatException("{}\nexception: at line {} inside {}\n{}\n",
                                                         insert_st(), "runtime exception: ", line_, location_)) { }

CE::Exceptions::invalid_args::invalid_args(const char* location_, uint32_t line_, const char* info_) noexcept
        : CE::Exceptions::exception_base(FormatException("{}\nexception: {} at line {} inside {}\n{}\n",
                                                         insert_st(), "runtime exception: ", line_, location_, info_)) { }

CE::Exceptions::runtime_exception::runtime_exception(const char* location_, uint32_t line_, const char* info_) noexcept
        : CE::Exceptions::exception_base(FormatException("{}\nexception: {} at line {} inside {}\n{}\n",
                                                         insert_st(), "runtime exception", line_, location_, info_)) { }

CE::Exceptions::runtime_exception::runtime_exception(const char* sub_type, const char* location_, uint32_t line_, const char* info_) noexcept
        : CE::Exceptions::exception_base(FormatException("{}\nexception: {}: {} at line {} inside {}\n{}\n",
                                                         insert_st(), "runtime exception", sub_type, line_, location_, info_)) { }

CE::Exceptions::bad_alloc::bad_alloc(const char* location_, uint32_t line_) noexcept
        : CE::Exceptions::runtime_exception("bad_alloc", location_, line_, "heap allocation failed") { }

CE::Exceptions::bad_request::bad_request(const char* location_, uint32_t line_, const char* info_) noexcept
        : CE::Exceptions::runtime_exception("bad_request", location_, line_, info_) { }

CE::Exceptions::failed_operation::failed_operation(const char* location_, uint32_t line_, const char* info_) noexcept
        : CE::Exceptions::runtime_exception("failed_operation", location_, line_, info_) { }
