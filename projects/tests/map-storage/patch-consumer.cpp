#include <cheryl/core/resources/map-storage/cell-patch.h>
#include "patch-fixture.h"

#include <cheryl/core/resources/map-storage/file-storage.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
    namespace Maps = CE::Maps;
    namespace Patches = CE::Maps::CellPatches;

    void require(bool condition, const std::string& detail) {
        if (!condition) throw std::runtime_error(detail);
    }

    std::string bytes(const std::filesystem::path& path) {
        const auto size = std::filesystem::file_size(path);
        require(size <= Maps::max_document_bytes, "sentinel exceeds byte budget");
        std::ifstream input;
        input.exceptions(std::ios::badbit | std::ios::failbit);
        input.open(path, std::ios::binary);
        std::string result(static_cast<std::size_t>(size), '\0');
        input.read(result.data(), static_cast<std::streamsize>(result.size()));
        input.close();
        return result;
    }

    void author(const std::filesystem::path& path) {
        {
            auto base = PatchFixture::base();
            Patches::Patch patch{Patches::capture(base),
                {{"soil", 1, 0, "garden:sand"}, {"markers", 0, 1, "garden:flower"}}};
            const auto result = Patches::apply(base, patch);
            require(result == PatchFixture::expected(), "two-cell result differs from complete literal");
            Maps::save(path, result);
            base = {};
            patch.edits.clear();
            patch.witness = {};
        }
        // No claim that a later failure rolls back the already successful save.
        require(Maps::load(path) == PatchFixture::expected(), "post-save complete reload differs");
        std::cout << "saved garden:north 2x3 layers [soil,markers]; gate soil(0,2), lookout markers(1,0)\n";
    }

    void verify(const std::filesystem::path& path, bool predecessor) {
        const auto expected = predecessor ? PatchFixture::predecessor() : PatchFixture::expected();
        auto loaded = Maps::load(path);
        require(loaded == expected, "fresh-process complete literal comparison failed");
        const auto independent = Maps::load(path);
        loaded.layers[0].cells[0] = "garden:changed";
        loaded.spawns.clear();
        require(independent == expected, "independent load aliases another result");
        std::cout << "fresh-process verified " << expected.id << "; all cells, empties, order and spawns\n";
    }

    void preserve(const std::filesystem::path& path) {
        const auto predecessor = PatchFixture::predecessor();
        Maps::save(path, predecessor);
        const auto before = bytes(path);
        auto changed = PatchFixture::base();
        changed.layers[0].cells[4] = "garden:clay"; // Same ID/dimensions, unrelated to the two edits.
        const auto patch = PatchFixture::patch();
        std::optional<Patches::Error> retained;
        try {
            // Argument evaluation rejects before save() is entered.
            Maps::save(path, Patches::apply(changed, patch));
        } catch (const Patches::Error& error) {
            retained = error;
        }
        require(retained && retained->code == Patches::Code::BaseMismatch,
                "expected exact-base refusal before file publication");
        require(retained->operation == "apply" && retained->location == "$.base",
                "exact-base diagnostic lost its owned context");
        require(before == bytes(path), "rejected patch changed predecessor bytes");
        require(Maps::load(path) == predecessor, "rejected patch changed predecessor values");
        require(changed.layers[0].cells[4] == "garden:clay" && patch == PatchFixture::patch(),
                "rejected patch changed its inputs");
        std::cerr << "expected BaseMismatch operation=" << retained->operation
                  << " location=" << retained->location << ": " << retained->what() << '\n';
        std::cout << "pre-publication refusal preserved exact garden:predecessor bytes and complete value\n";
    }

    void diff_author(const std::filesystem::path& path) {
        {
            auto base = PatchFixture::base();
            auto target = PatchFixture::expected();
            auto patch = Patches::diff(base, target);
            const auto result = Patches::apply(base, patch);
            require(result == PatchFixture::expected(), "diff result differs from complete literal");
            Maps::save(path, result);
            base = {};
            target = {};
            patch = {};
        }
        require(Maps::load(path) == PatchFixture::expected(), "post-save diff reload differs");
        std::cout << "diff/apply saved complete garden:north literal; source and preparation destroyed\n";
    }

    void diff_preserve(const std::filesystem::path& path) {
        const auto predecessor = PatchFixture::predecessor();
        Maps::save(path, predecessor);
        const auto before = bytes(path);
        const auto base = PatchFixture::base();
        auto target = PatchFixture::expected();
        target.spawns[0].x = 1; // Valid value, forbidden metadata change.
        const auto original = target;
        std::optional<Patches::Error> retained;
        try {
            Maps::save(path, Patches::apply(base, Patches::diff(base, target)));
        } catch (const Patches::Error& error) {
            retained = error;
        }
        require(retained && retained->code == Patches::Code::UnsupportedChange &&
                    retained->operation == "diff" && retained->location == "$.target.spawns[0].x",
                "expected unsupported diff preparation refusal");
        require(target == original && base == PatchFixture::base(), "rejected preparation changed inputs");
        require(bytes(path) == before && Maps::load(path) == predecessor,
                "rejected preparation changed exact predecessor bytes/value");
        std::cerr << "expected UnsupportedChange operation=diff location=" << retained->location << '\n';
        std::cout << "diff preparation refused before publication; exact garden:predecessor survived\n";
    }
}

int main(int argc, char** argv) {
    if (argc != 3 || std::string_view(argv[2]).empty()) {
        std::cerr << "usage: map_storage_patch_consumer <author|verify|preserve|verify-predecessor|"
                     "diff-author|verify-diff|diff-preserve> <path>\n";
        return 64;
    }
    try {
        const std::string_view command = argv[1];
        const std::filesystem::path path{argv[2]};
        if (command == "author") author(path);
        else if (command == "verify") verify(path, false);
        else if (command == "preserve") preserve(path);
        else if (command == "verify-predecessor") verify(path, true);
        else if (command == "diff-author") diff_author(path);
        else if (command == "verify-diff") verify(path, false);
        else if (command == "diff-preserve") diff_preserve(path);
        else {
            std::cerr << "unknown private consumer command\n";
            return 64;
        }
        std::cout.flush();
        if (!std::cout) throw std::runtime_error("output failed after operation; a successful save remains committed");
        return 0;
    } catch (const Patches::Error& error) {
        std::cerr << "patch category=" << static_cast<int>(error.code) << " operation=" << error.operation
                  << " location=" << error.location << ": " << error.what() << '\n';
        if (error.cause) std::cerr << "codec cause: " << error.cause->what() << '\n';
        return 2;
    } catch (const Maps::Error& error) {
        std::cerr << "storage category=" << static_cast<int>(error.code) << " operation=" << error.operation
                  << " origin=" << error.origin << " location=" << error.location << ": " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "private consumer failure: " << error.what() << '\n';
        return 1;
    }
}
