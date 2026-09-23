#include <gtest/gtest.h>
#include <ctti/type_id.hpp>
#include <internals.h>
#include <core/logging.h>
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
    // Resolve the registered "cheryl" logger through both its public and base types.
    ASSERT_TRUE(ctti::type_id(CELog::get()) == ctti::type_id(Logger<ce_log_name>::get()));

    // A second logger with the same name cannot register independently.
    bool ethrow = false;
    try {
        ASSERT_TRUE(ctti::type_id(CELog::get()) == ctti::type_id(DupeLog::get()));
    } catch (...) {
        ethrow = true;
    }
    ASSERT_TRUE(ethrow);

    // A subtype shares CELog's instance and ID; a differently named logger has
    // a separate ID and type, even though it uses the same Logger template.
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
    // Create a dedicated logger and write its messages before destroying it.
    Log<ltester>* log = new Log<ltester>();
    auto path = log->get_file_path();
    test_file_logging(log);
    delete log;

    // Read the closed log file to check which messages reached the sink.
    check_file_logging(path);
}

void test_celog_console();
TEST(logging, CELog) {
    // The shared logger should write to the appropriate console streams.
    auto path = CELog::get_file_path();
    test_celog_console();

    // Write through the same logger's file sink, then inspect its output.
    test_file_logging(&CELog::get());
    check_file_logging(path);

    // Restore the shared logger after checking its file output.
    CELog::reopen();
}

template<typename L>
void test_file_logging(L* log) {
    // Enable all levels, write one message per level, and close the file sink
    // before the caller reads it back.
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
    // Read the closed file into a null-terminated buffer for pattern matching.
    ASSERT_FALSE(path.empty());
    std::fstream logfile(path);
    ASSERT_TRUE(logfile.is_open());
    logfile.seekg(0, std::ios::end);
    auto file_size = static_cast<int64_t>(logfile.tellg()) + 1;
    logfile.seekg(0, std::ios::beg);
    ASSERT_TRUE(file_size > 0);
    auto buffer = new char[file_size];
    logfile.read(buffer, file_size); // Includes one byte beyond EOF for the terminator below.
    buffer[file_size-1] = 0;
    std::string file_content(buffer);
    delete[] buffer;

    // Require all three message fragments in the recorded file.
    ASSERT_TRUE(std::regex_search(file_content, std::regex("pointers.situation")));
    ASSERT_TRUE(std::regex_search(file_content, std::regex("second.log.line")));
    ASSERT_TRUE(std::regex_search(file_content, std::regex("an.error")));
    //ASSERT_TRUE(std::regex_search(buffer, std::regex("software")));
}

void test_celog_console() {
    //std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    // Capture stdout before info/warn and stderr before error, then flush the
    // streams so their contents can be inspected.
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

    // Confirm each level reached its intended stream and not the other one.
    ASSERT_TRUE(std::regex_search(out_str_stderr, std::regex("error")));
    ASSERT_TRUE(std::regex_search(out_str_stdout, std::regex("info")));
    ASSERT_TRUE(std::regex_search(out_str_stdout, std::regex("warn")));
    ASSERT_FALSE(std::regex_search(out_str_stdout, std::regex("error")));
    ASSERT_FALSE(std::regex_search(out_str_stderr, std::regex("info")));
    ASSERT_FALSE(std::regex_search(out_str_stderr, std::regex("warn")));
}

