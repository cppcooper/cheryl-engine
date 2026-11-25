#pragma once

#include <cheryl/tools/console/scenario.h>
#include <filesystem>

namespace CE::Tools::Console::Runtime {
    namespace Core = CE::Tools::Console;
    namespace Script = Core::Scenario;
    inline constexpr std::size_t max_output_bytes = 4 * 1024 * 1024;
    CE_CONSOLE_API std::string content_id(std::string_view bytes);
    CE_CONSOLE_API std::string quote(std::string_view text);
    CE_CONSOLE_API std::string typed(const Core::Value&, const Core::Schema&);
    CE_CONSOLE_API std::string_view label(Script::Disposition) noexcept;
    CE_CONSOLE_API std::string read_source(const std::filesystem::path& root, std::string_view relative);
    /** Confined POSIX descriptor I/O or Windows handle-checked drive-path I/O.
     * Roots must already exist, have no symlink components, and be host-approved.
     * Output uses a fresh single-component run directory. The host owns the root;
     * this is not a sandbox against a hostile process able to rename its ancestors.
     * Windows rejects UNC/device roots, reparse points, reserved DOS names, and
     * trailing dots. Directory handles deny write/delete sharing through cleanup;
     * readers deny write/delete sharing and reject multiply linked files.
     * complete is an exclusive marker published only after both artifacts flush.
     * Windows closes artifact writers before publication; no writes follow finish.
     * FlushFileBuffers is checked, but directory-entry crash durability is not
     * promised on Windows. Other platforms refuse instead of using stream I/O.
     */
    class CE_TYPE_API Report {
        struct State;
        std::unique_ptr<State> state_;
    public:
        CE_CONSOLE_API Report(const std::filesystem::path& root, std::string run_id, std::size_t capacity);
        Report(const Report&) = delete;
        Report& operator=(const Report&) = delete;
        CE_CONSOLE_API ~Report();
        CE_CONSOLE_API void transcript(std::string_view);
        CE_CONSOLE_API void running(std::string_view json);
        CE_CONSOLE_API void finish(std::string_view json, std::string_view source_id, std::string_view build_id);
        CE_CONSOLE_API bool invalidate() noexcept;
        CE_CONSOLE_API const std::string& run_id() const noexcept;
    };
    /** Checks a finalized successful report and artifact identities, not merely
     * persistence. False includes durable failed/invalid reports. Content IDs
     * detect accidental mismatch; FNV-1a is not an authentication mechanism.
     */
    CE_CONSOLE_API bool completed_report(const std::filesystem::path& output_root, std::string_view run_id,
                          std::string_view source_id, std::string_view build_id);
}
