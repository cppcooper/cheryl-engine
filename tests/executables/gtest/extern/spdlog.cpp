#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

TEST(externlibs, spdlog){
   testing::internal::CaptureStdout();
   testing::internal::CaptureStderr();
   auto console = spdlog::stdout_color_mt("console");
   auto err_logger = spdlog::stderr_color_mt("stderr");
   console->set_pattern("%v");
   err_logger->set_pattern("%v");

   // print messages
   std::string in_str_stdout = "hello world";
   std::string in_str_stderr = "goodbye world";
   console->info(in_str_stdout);
   err_logger->error(in_str_stderr);

   // retrieve messages
   std::string out_str_stdout = testing::internal::GetCapturedStdout();
   std::string out_str_stderr = testing::internal::GetCapturedStderr();

   // retrieved messages automatically have '\n' appended
   out_str_stdout.erase(std::remove(out_str_stdout.begin(),out_str_stdout.end(),'\n'), out_str_stdout.end());
   out_str_stderr.erase(std::remove(out_str_stderr.begin(),out_str_stderr.end(),'\n'), out_str_stderr.end());

   // now we can test
   ASSERT_STREQ(in_str_stdout.c_str(), out_str_stdout.c_str());
   ASSERT_STREQ(in_str_stderr.c_str(), out_str_stderr.c_str());
}