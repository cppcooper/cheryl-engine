#include "cli-diagnostics.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <sstream>

TEST(cli_output, primary_details) {
    const CE::Maps::Error error(CE::Maps::Code::Read, "parse", "fixture/input.map.json", "$.layers[1].cells[2]",
                                  "primary read failure", 27, {EIO, std::generic_category()});
    std::ostringstream output;
    MapFixture::Cli::print_error(output, error);
    const auto text = output.str();
    EXPECT_NE(text.find("category=Read"), std::string::npos);
    EXPECT_NE(text.find("operation=\"parse\""), std::string::npos);
    EXPECT_NE(text.find("origin=\"fixture/input.map.json\""), std::string::npos);
    EXPECT_NE(text.find("location=\"$.layers[1].cells[2]\""), std::string::npos);
    EXPECT_NE(text.find("byte_offset=27"), std::string::npos);
    EXPECT_NE(text.find("io_error=generic:" + std::to_string(EIO)), std::string::npos);
    EXPECT_NE(text.find(error.io_error.message()), std::string::npos);
    EXPECT_NE(text.find(error.what()), std::string::npos);
    EXPECT_EQ(text.find("cleanup_error="), std::string::npos);
}

TEST(cli_output, cleanup_keeps_primary) {
    CE::Maps::Error error(CE::Maps::Code::Replace, "save", "fixture/output.map.json", "$replace",
                            "primary replace failure", {}, {EACCES, std::generic_category()});
    error.cleanup_error = {EBUSY, std::generic_category()};
    error.staging_leftover = "fixture/.map-storage-owned";
    std::ostringstream output;
    MapFixture::Cli::print_error(output, error);
    const auto text = output.str();
    EXPECT_NE(text.find("category=Replace"), std::string::npos);
    EXPECT_NE(text.find("location=\"$replace\""), std::string::npos);
    EXPECT_NE(text.find("io_error=generic:" + std::to_string(EACCES)), std::string::npos);
    EXPECT_NE(text.find(error.what()), std::string::npos);
    EXPECT_NE(text.find("cleanup_error=generic:" + std::to_string(EBUSY)), std::string::npos);
    EXPECT_NE(text.find("staging_leftover=\"fixture/.map-storage-owned\""), std::string::npos);
    EXPECT_LT(text.find(error.what()), text.find("cleanup_error="));
    EXPECT_EQ(text.find("byte_offset="), std::string::npos);
    EXPECT_EQ(error.io_error.value(), EACCES);
    EXPECT_EQ(error.location, "$replace");
}
