#pragma once

#include <cheryl/tools/console/session.h>
#include <cheryl/tools/console/parser.h>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace CE::Tools::Console::Presentation {
    namespace Core = CE::Tools::Console;
    /** Retained owner bridge. All methods run on the Model caller/Session owner.
     * It owns or retains the Registry, Context gate, and Session through close.
     * close must stop admission without waiting; retained results stay readable.
     */
    class CE_CONSOLE_API Connection {
    public:
        virtual ~Connection();
        virtual Core::Registry& registry() noexcept = 0;
        virtual Core::Context context() const noexcept = 0;
        virtual Core::Session& session() noexcept = 0;
        virtual std::string scope() const = 0;
        virtual void close() noexcept = 0;
    };

    struct Entry {
        std::uint64_t id;
        std::string line;
        Core::Schema schema;
        Core::Result result;
    };

    class CE_TYPE_API Model {
        std::shared_ptr<Connection> client_;
        std::vector<Entry> entries_;
        std::vector<std::string> history_;
        std::size_t history_bytes_ = 0;
        std::size_t history_cursor_ = 0;
        std::string draft_, notice_, scope_;
        std::optional<Core::Failure> failure_;
        std::uint64_t next_id_ = 1;
        bool closed_ = false, history_trimmed_ = false;

        void remember(std::string_view line);
        void refuse(std::string_view line, Core::Failure failure);
        void refresh();

    public:
        static constexpr std::size_t command_bytes = Core::max_line;
        static constexpr std::size_t history_count = 32, history_bytes = 32768;
        static constexpr std::size_t output_bytes = 65536;
        static constexpr std::size_t suggestion_count = 12, suggestion_bytes = 4096;

        CE_CONSOLE_API explicit Model(std::shared_ptr<Connection> connection);
        CE_CONSOLE_API virtual ~Model();
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        CE_CONSOLE_API bool submit(std::string_view line);
        CE_CONSOLE_API void pump();
        CE_CONSOLE_API void close() noexcept;
        CE_CONSOLE_API void help(std::string_view command = {});
        CE_CONSOLE_API std::vector<std::string> suggestions(std::string_view line);
        CE_CONSOLE_API std::string complete(std::string_view line);
        CE_CONSOLE_API std::string previous(std::string_view current);
        CE_CONSOLE_API std::string next();
        CE_CONSOLE_API std::string text() const;
        std::string_view scope() const noexcept { return scope_; }
        std::span<const Entry> entries() const noexcept { return entries_; }
        std::span<const std::string> history() const noexcept { return history_; }
        const std::optional<Core::Failure>& failure() const noexcept { return failure_; }
        bool accepting() const noexcept { return !closed_ && client_->session().accepting(); }
    };

    CE_CONSOLE_API std::string escaped(std::string_view text);
    CE_CONSOLE_API std::string display_text(std::string_view text);
    CE_CONSOLE_API std::string_view stage_name(Core::Stage stage) noexcept;
    CE_CONSOLE_API std::string_view code_name(Core::Code code) noexcept;
}
