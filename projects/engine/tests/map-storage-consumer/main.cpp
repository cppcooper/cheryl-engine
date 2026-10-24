#include <cheryl/core/resources/map-storage/file-storage.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

namespace {
    namespace Maps = CE::Maps;

    Maps::Snapshot authored() {
        return {
            .id = "garden:north", .width = 2, .height = 3,
            .layers = {
                {.id = "soil", .cells = {"garden:earth", "garden:stone", "garden:earth",
                                        "garden:water", "garden:earth", "garden:earth"}},
                {.id = "markers", .cells = {std::nullopt, "garden:flag", std::nullopt,
                                           std::nullopt, "garden:tree", std::nullopt}}
            },
            .spawns = {{.name = "gate", .layer = "soil", .x = 0, .y = 2},
                       {.name = "lookout", .layer = "markers", .x = 1, .y = 0}}
        };
    }

    Maps::Snapshot predecessor() {
        auto result = authored();
        result.id = "garden:predecessor";
        result.layers[0].cells[1] = "garden:sand";
        return result;
    }

    void require(bool condition, const char* detail) {
        if (!condition)
            throw std::runtime_error(detail);
    }

    void memory() {
        Maps::Snapshot first, second;
        {
            auto source = authored();
            auto bytes = Maps::encode(source);
            std::istringstream input(bytes);
            first = Maps::parse(input, "memory:garden-first");
            std::istringstream other(bytes);
            second = Maps::parse(other, "memory:garden-second");
            source.layers[0].cells[0].reset();
            source.spawns.clear();
            bytes.assign("destroyed input");
        }
        require(first == authored() && second == authored(), "encode/parse retained source storage");
        auto copy = first;
        first.id = "garden:edited";
        first.layers[0].cells[0].reset();
        first.spawns[0].x = 1;
        require(second == authored() && copy == authored(), "independent snapshots changed together");

        std::optional<Maps::Error> retained;
        {
            std::filesystem::path origin = "memory:discarded-origin";
            std::istringstream broken("{");
            try {
                second = Maps::parse(broken, origin);
            } catch (const Maps::Error& error) {
                retained = error;
            }
            origin.clear();
        }
        require(retained.has_value(), "truncated input unexpectedly parsed");
        require(retained->code == Maps::Code::Malformed && retained->operation == "parse" &&
                    retained->origin == "memory:discarded-origin" && retained->location.starts_with("$") &&
                    retained->byte_offset.has_value() && *retained->byte_offset > 0 &&
                    std::string_view(retained->what()).find("memory:discarded-origin") != std::string_view::npos &&
                    std::string_view(retained->what()).find(retained->location) != std::string_view::npos,
                "diagnostic did not retain owned category/origin/location/offset");
        require(second == authored(), "failed parse changed a previous owned snapshot");
        std::cout << "owned error: Malformed operation=" << retained->operation << " origin=" << retained->origin
                  << " location=" << retained->location << " byte_offset=" << *retained->byte_offset << '\n';
    }

    void validation() {
        const auto expected = authored();
        Maps::validate(expected);
        require(expected == authored(), "validation mutated the complete snapshot");
        std::optional<Maps::Error> retained;
        {
            auto invalid = authored();
            invalid.layers[1].cells[2] = "not-qualified";
            const auto before = invalid;
            try {
                Maps::validate(invalid);
            } catch (const Maps::Error& error) {
                retained = error; // Owning copy across the actual producer boundary.
            }
            require(invalid == before, "failed validation mutated the input");
        }
        require(retained.has_value(), "invalid snapshot unexpectedly validated");
        const auto copied = *retained;
        retained.reset();
        require(copied.code == Maps::Code::InvalidValue && copied.operation == "validate" &&
                    copied.origin == "<snapshot>" && copied.location == "$.layers[1].cells[2]" &&
                    !copied.byte_offset && !copied.io_error && !copied.cleanup_error && copied.staging_leftover.empty() &&
                    std::string_view(copied.what()).find("validate '<snapshot>'") != std::string_view::npos,
                "owned validation error lost context after source/error destruction");
        std::cout << "validate: complete garden unchanged; owned InvalidValue operation=" << copied.operation
                  << " origin=" << copied.origin << " location=" << copied.location << '\n';
    }

    void author(const std::filesystem::path& path) {
        {
            auto source = authored();
            Maps::save(path, source);
            source.id = "garden:destroyed";
            source.layers.clear();
            source.spawns.clear();
        }
        // The independent verify invocation reconstructs every literal value.
        std::cout << "authored garden:north 2x3 [soil,markers] gate@soil(0,2) lookout@markers(1,0)\n";
    }

    void verify(const std::filesystem::path& path, bool old) {
        const auto expected = old ? predecessor() : authored();
        auto first = Maps::load(path);
        auto second = Maps::load(path);
        require(first == expected && second == expected, "loaded snapshot differs from complete literal");
        first.layers[0].cells[0].reset();
        first.spawns.clear();
        require(second == expected, "loads share mutable values");
        std::cout << "verified " << second.id << " 2x3 [soil,markers], all cells and both spawns\n";
    }

    std::string read_bytes(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        input.exceptions(std::ios::badbit | std::ios::failbit);
        std::string bytes(std::filesystem::file_size(path), '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        input.close();
        return bytes;
    }

    void preserve(const std::filesystem::path& path) {
        const auto old = predecessor();
        Maps::save(path, old);
        const auto before = read_bytes(path);
        auto candidate = authored();
        candidate.layers[0].cells.pop_back(); // Distinct, deliberately invalid B.
        std::optional<Maps::Error> retained;
        try {
            Maps::save(path, candidate);
        } catch (const Maps::Error& error) {
            retained = error;
        }
        require(retained.has_value(), "invalid candidate unexpectedly saved");
        require(retained->code == Maps::Code::InvalidValue && retained->operation == "encode" &&
                    retained->origin == "<snapshot>" && retained->location == "$.layers[0].cells" &&
                    !retained->io_error && !retained->cleanup_error && retained->staging_leftover.empty(),
                "failure did not identify pre-staging validation");
        require(read_bytes(path) == before && Maps::load(path) == old, "failed save changed predecessor");
        std::cout << "rejected invalid B before staging; exact predecessor bytes and complete values preserved\n";
    }
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "memory") {
            memory();
            std::cout << "owned codec values, independent copies and retained error verified\n";
        } else if (argc == 2 && std::string_view(argv[1]) == "validation") {
            validation();
        } else if (argc == 3 && argv[2][0] != '\0') {
            const std::string_view command(argv[1]);
            const std::filesystem::path path(argv[2]);
            if (command == "author")
                author(path);
            else if (command == "verify" || command == "verify-predecessor")
                verify(path, command == "verify-predecessor");
            else if (command == "preserve")
                preserve(path);
            else
                throw std::runtime_error("unknown consumer command");
        } else {
            throw std::runtime_error("consumer expects memory/validation, or author/verify/verify-predecessor/preserve <file>");
        }
        std::cout.flush();
        require(bool(std::cout), "consumer output failed");
        return 0;
    } catch (const Maps::Error& error) {
        std::cerr << "storage category=" << static_cast<int>(error.code) << " operation=" << error.operation
                  << " origin=" << error.origin << " location=" << error.location << " detail=" << error.what()
                  << " io_error=" << error.io_error << " cleanup_error=" << error.cleanup_error
                  << " staging_leftover=" << error.staging_leftover << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "consumer failure: " << error.what() << '\n';
        return 1;
    }
}
