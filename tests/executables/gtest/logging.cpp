#include <gtest/gtest.h>
#include <ctti/type_id.hpp>
#include <internal/internals.h>
#include <logging.h>
#include <regex>
#include <fstream>
#include <iostream>

using namespace CE;

constexpr char flog_name[] = "fake";
struct FLog : Logger<flog_name> {
    FLog() = default;
};

constexpr char dupe_name[] = "cheryl";
struct DupeLog : Logger<dupe_name> {
    DupeLog() = default;
};

struct ProperlyDupedLog : CELog {
    ProperlyDupedLog() = default;
};

TEST(logging, types) {
    ASSERT_TRUE(ctti::type_id(CELog::get()) == ctti::type_id(Logger<ce_log_name>::get()));
    // Log "cheryl" already exists ^^^^^^^^ here where it gets instantiated
    // Instantiating DupeLog will throw an exception due to this
    bool ethrow = false;
    try {
        // can't be asserted true, because the evaluation doesn't complete.. an exception is thrown instead
        // would be true if it weren't an exception event though..
        ASSERT_TRUE(ctti::type_id(CELog::get()) == ctti::type_id(DupeLog::get()));
    } catch (...) {
        ethrow = true;
    }
    ASSERT_TRUE(ethrow);
    ASSERT_TRUE(ctti::type_id(CELog::get()) == ctti::type_id(ProperlyDupedLog::get()));
    ASSERT_TRUE(CELog::get().get_log_id() == Logger<ce_log_name>::get().get_log_id());
    ASSERT_TRUE(CELog::get().get_log_id() == Logger<ce_log_name>::get().get_log_id());
    ASSERT_TRUE(CELog::get().get_log_id() != FLog::get().get_log_id());
    ASSERT_TRUE(ctti::type_id<CELog>() != ctti::type_id<FLog>());
    ASSERT_TRUE(ctti::type_id<CELog>() != ctti::type_id<DupeLog>());
}

template<typename L>
void test_file_logging(L* log);
void check_file_logging(fs::path& path);
constexpr char ltester[] = "LogTester";
TEST(logging, log) {
    Log<ltester>* log = new Log<ltester>();
    auto path = log->get_file_path();
    test_file_logging(log);
    delete log;
    check_file_logging(path);
}

void test_celog_console();
TEST(logging,CELog) {
    auto path = CELog::get_file_path();
    test_celog_console();
    test_file_logging(&CELog::get());
    check_file_logging(path);
}

template<typename L>
void test_file_logging(L* log) {
    log->set_level_filesink(spdlog::level::trace);
    log->warn("We're in a pointers situation.");
    log->info("This is the second log line, from this test.");
    log->error("This is not an error! [error]");
    // todo: macro enable tracing??
    //log->trace("we're tracing a software..");
    log->flush();
    log->close();
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void check_file_logging(fs::path& path) {
    ASSERT_FALSE(path.empty());
    std::fstream logfile(path);
    ASSERT_TRUE(logfile.is_open());
    logfile.seekg(0, std::ios::end);
    auto file_size = logfile.tellg();
    logfile.seekg(0, std::ios::beg);
    ASSERT_TRUE(file_size > 0);
    char* buffer = new char[file_size];
    logfile.read(buffer, file_size); // this read operation "fails" (don't try to assert it)
    std::string file_content(buffer);
    delete[] buffer;
    // verify log contents
    ASSERT_TRUE(std::regex_search(file_content, std::regex("pointers.situation")));
    ASSERT_TRUE(std::regex_search(file_content, std::regex("second.log.line")));
    ASSERT_TRUE(std::regex_search(file_content, std::regex("an.error")));
    //ASSERT_TRUE(std::regex_search(buffer, std::regex("software")));
}

void test_celog_console() {
    //std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    testing::internal::CaptureStdout();
    CELog::info("info");
    CELog::warn("warn");
    testing::internal::CaptureStderr();
    CELog::error("error");
    std::cout.flush();
    std::cerr.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    //std::this_thread::sleep_for(std::chrono::milliseconds(15));
    std::string out_str_stdout = testing::internal::GetCapturedStdout();
    std::string out_str_stderr = testing::internal::GetCapturedStderr();
    std::cout << "str_stdout: " << out_str_stdout << std::endl;
    std::cout << "str_stderr: " << out_str_stderr << std::endl;
    ASSERT_TRUE(std::regex_search(out_str_stderr, std::regex("error")));
    ASSERT_TRUE(std::regex_search(out_str_stdout, std::regex("info")));
    ASSERT_TRUE(std::regex_search(out_str_stdout, std::regex("warn")));
    ASSERT_FALSE(std::regex_search(out_str_stdout, std::regex("error")));
    ASSERT_FALSE(std::regex_search(out_str_stderr, std::regex("info")));
    ASSERT_FALSE(std::regex_search(out_str_stderr, std::regex("warn")));
}

