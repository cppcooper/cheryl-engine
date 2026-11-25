#include <cheryl/tools/console/model.h>

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

namespace CE::Tools::Console::Presentation {
    Connection::~Connection() = default;

    namespace {
        constexpr std::array builtins = {
            std::pair<std::string_view, std::string_view>{"clear", "Clear the console output."},
            std::pair<std::string_view, std::string_view>{"echo", "echo <text> - Print text to the console."},
            std::pair<std::string_view, std::string_view>{"exit", "Close the application."},
            std::pair<std::string_view, std::string_view>{"find", "find <text> - Search command names and descriptions."},
            std::pair<std::string_view, std::string_view>{"help", "help [command] - List commands or show command usage."},
            std::pair<std::string_view, std::string_view>{"quit", "Close the application."}
        };

        bool builtin(std::string_view name) {
            return std::ranges::any_of(builtins, [name](const auto& command) { return command.first == name; });
        }

        std::string_view trimmed(std::string_view line) {
            const auto first = line.find_first_not_of(" \t");
            if (first == std::string_view::npos)
                return {};
            const auto last = line.find_last_not_of(" \t");
            return line.substr(first, last - first + 1);
        }

        Core::Failure original_failure(Core::Failure failure, std::size_t offset, std::size_t command_size, std::size_t target_size) {
            const auto position = [&](std::size_t byte) {
                if (byte <= offset)
                    return byte;
                if (byte >= offset + target_size)
                    return byte - target_size + command_size;
                return offset + std::min(byte - offset, command_size);
            };
            const auto end = position(failure.offset + failure.length);
            failure.offset = position(failure.offset);
            failure.length = end - failure.offset;
            return failure;
        }

        bool contains(std::string_view text, std::string_view search) {
            const auto lower = [](unsigned char byte) { return byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte; };
            return std::ranges::search(text, search, [lower](char left, char right) {
                return lower(static_cast<unsigned char>(left)) == lower(static_cast<unsigned char>(right));
            }).begin() != text.end();
        }

        Core::Checked<std::string> echo_text(std::string_view text, std::size_t offset) {
            std::string result;
            bool quoted = false, escaping = false;
            for (std::size_t index = 0; index < text.size(); ++index) {
                const char byte = text[index];
                if (escaping) {
                    if (byte != '"' && byte != '\\')
                        return std::unexpected(Core::Failure{Core::Code::Syntax, offset + index, 1});
                    result += byte;
                    escaping = false;
                } else if (quoted && byte == '\\')
                    escaping = true;
                else if (byte == '"')
                    quoted = !quoted;
                else
                    result += byte;
            }
            if (quoted || escaping)
                return std::unexpected(Core::Failure{Core::Code::Syntax, offset, text.size()});
            return result;
        }

        std::size_t prefix_size(std::string_view text, std::size_t bytes) {
            bytes = std::min(bytes, text.size());
            while (bytes && bytes < text.size() && (static_cast<unsigned char>(text[bytes]) & 0xc0) == 0x80)
                --bytes;
            return bytes;
        }

        bool append(std::string& output, std::string_view text, std::size_t limit = Model::output_bytes) {
            const auto count = prefix_size(text, limit > output.size() ? limit - output.size() : 0);
            output.append(text.substr(0, count));
            return count == text.size();
        }

        bool unsafe(std::uint32_t codepoint) {
            return codepoint < 32 || (codepoint >= 127 && codepoint <= 159) || codepoint == 0x2028 || codepoint == 0x2029 ||
                   (codepoint >= 0x202a && codepoint <= 0x202e) || (codepoint >= 0x2066 && codepoint <= 0x2069);
        }

        std::pair<std::uint32_t, std::size_t> scalar(std::string_view text, std::size_t offset) {
            const auto first = static_cast<unsigned char>(text[offset]);
            const std::size_t length = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
            std::uint32_t codepoint = first & (length == 1 ? 0x7f : length == 2 ? 0x1f : length == 3 ? 0x0f : 0x07);
            for (std::size_t index = 1; index < length; ++index)
                codepoint = (codepoint << 6) | (static_cast<unsigned char>(text[offset + index]) & 0x3f);
            return {codepoint, length};
        }

        std::optional<Core::Failure> input_failure(std::string_view line) {
            if (line.size() > Model::command_bytes)
                return Core::Failure{Core::Code::Limit, Model::command_bytes, line.size() - Model::command_bytes};
            if (!Core::valid_utf8(line))
                return Core::Failure{Core::Code::Encoding, 0, line.size()};
            for (std::size_t offset = 0; offset < line.size();) {
                const auto [codepoint, length] = scalar(line, offset);
                if (unsafe(codepoint))
                    return Core::Failure{Core::Code::Encoding, offset, length};
                offset += length;
            }
            return {};
        }

        std::string type_text(const Core::Schema& schema) {
            using enum Core::Type;
            switch (schema.type) {
            case Boolean: return "boolean";
            case Unsigned: return std::format("unsigned [0, {}] {}", schema.unsigned_max, schema.unit);
            case Integer: return std::format("integer [{}, {}] {}", schema.integer_min, schema.integer_max, schema.unit);
            case Scalar: return "scalar " + schema.unit;
            case Text: return std::format("UTF-8 text ({} bytes)", schema.text_limit);
            case Handle: return "handle/alias: " + schema.domain;
            case Record: return "record";
            case List: return std::format("list ({} entries)", schema.count);
            case Enumeration: {
                std::string choices = "one of:";
                for (const auto& choice : schema.choices)
                    append(choices, " " + display_text(choice));
                return choices;
            }
            }
            return "unknown";
        }

        bool value_text(std::string& output, const Core::Value& value, const Core::Schema& schema,
                        std::size_t limit, std::size_t depth = 0) {
            using enum Core::Type;
            if (output.size() >= limit)
                return false;
            if (depth > Core::max_depth) {
                append(output, "[depth limit]", limit);
                return false;
            }
            bool complete = true;
            const auto put = [&](std::string_view text) { complete &= append(output, text, limit); };
            switch (value.type) {
            case Boolean: put(std::get<bool>(value.data) ? "true" : "false"); break;
            case Unsigned: put(std::to_string(std::get<std::uint64_t>(value.data))); break;
            case Integer: put(std::to_string(std::get<std::int64_t>(value.data))); break;
            case Scalar: put(std::format("{}", std::get<double>(value.data))); break;
            case Text: put("\"" + display_text(std::get<std::string>(value.data)) + "\""); break;
            case Enumeration: {
                const auto index = std::get<Core::Enum>(value.data).index;
                put(index < schema.choices.size() ? display_text(schema.choices[index]) : std::to_string(index));
                break;
            }
            case Handle: {
                const auto& handle = std::get<Core::Handle>(value.data);
                put(std::format("{}#{}", display_text(handle.domain()), handle.key()));
                break;
            }
            case Record:
            case List: {
                const auto& children = std::get<Core::Value::Children>(value.data);
                put(value.type == Record ? "{" : "[");
                for (std::size_t index = 0; index < children.size() && index < Core::max_nodes && output.size() < limit; ++index) {
                    if (index)
                        put(", ");
                    if (value.type == Record && index < schema.fields.size())
                        put(display_text(schema.fields[index]) + ": ");
                    const auto child = value.type == List ? 0 : index;
                    complete &= value_text(output, children[index], child < schema.children.size() ? schema.children[child] : Core::Schema{}, limit, depth + 1);
                }
                put(value.type == Record ? "}" : "]");
                break;
            }
            }
            return complete;
        }

        std::string quote(std::string_view token) {
            if (token.find_first_of(" \t\"'\\") == std::string_view::npos)
                return std::string{token};
            std::string result = "\"";
            for (const auto character : token) {
                if (character == '\\' || character == '"')
                    result += '\\';
                result += character;
            }
            return result + '"';
        }
    }

    Model::Model(std::shared_ptr<Connection> connection) : client_(std::move(connection)) {
        if (!client_) throw std::invalid_argument("Console Model requires an owned connection");
        entries_.reserve(Core::max_requests);
        scope_ = client_->scope();
        notice_ = "Type help for a list of commands.\n";
    }
    Model::~Model() { close(); }

    std::string_view Model::command_target(std::string_view command) const noexcept {
        auto& registry = client_->registry();
        if (registry.registered(command))
            return command;
        if ((command == "quit" || command == "exit") && registry.registered("app.quit"))
            return "app.quit";
        if (command == "exit" && registry.registered("quit"))
            return "quit";
        if (command == "echo" && registry.registered("app.echo"))
            return "app.echo";
        return command;
    }

    std::vector<Core::Descriptor> Model::commands() const {
        auto& registry = client_->registry();
        const auto context = client_->context();
        auto descriptors = registry.commands(context);
        for (const auto& [name, description] : builtins) {
            const auto target = command_target(name);
            if (registry.registered(target)) {
                if (target != name)
                    if (auto descriptor = registry.describe(target, context)) {
                        descriptor->name = name;
                        descriptors.push_back(std::move(*descriptor));
                    }
            } else if (name != "quit" && name != "exit") {
                Core::Descriptor descriptor;
                descriptor.name = name;
                descriptor.help = description;
                descriptors.push_back(std::move(descriptor));
            }
        }
        std::ranges::sort(descriptors, {}, &Core::Descriptor::name);
        return descriptors;
    }

    void Model::remember(std::string_view line) {
        if (history_.empty() || history_.back() != line) {
            while (!history_.empty() && (history_.size() >= history_count || history_bytes_ + line.size() > history_bytes)) {
                history_bytes_ -= history_.front().size();
                history_.erase(history_.begin());
                history_trimmed_ = true;
            }
            history_.emplace_back(line);
            history_bytes_ += line.size();
        }
        history_cursor_ = history_.size();
        draft_.clear();
    }

    void Model::refuse(std::string_view line, Core::Failure failure) {
        failure_ = failure;
        notice_.clear();
        std::string message;
        if (!line.empty()) {
            message = "] ";
            append(message, display_text(line.substr(0, prefix_size(line, command_bytes))), 8192 - 256);
            message += '\n';
        }
        append(message, std::format("Error: {} at UTF-8 bytes [{}, {})\n", code_name(failure.code), failure.offset,
                                   failure.offset + failure.length), 8192);
        if (line.size() > command_bytes)
            append(message, "[command exceeds capacity]\n", 8192);
        write(std::move(message));
    }

    void Model::write(std::string text, std::uint64_t request) {
        while (!display_.empty() && (display_.size() >= 512 || display_bytes_ + text.size() > output_bytes)) {
            display_bytes_ -= display_.front().text.size();
            display_.erase(display_.begin());
            output_trimmed_ = true;
        }
        display_bytes_ += text.size();
        display_.push_back({request, std::move(text)});
    }

    bool Model::submit(std::string_view line) {
        if (const auto failure = input_failure(line)) {
            refuse(line, *failure);
            return false;
        }
        if (!accepting()) {
            refuse(line, {Core::Code::Expired});
            return false;
        }
        const auto command_line = trimmed(line);
        if (command_line.empty())
            return true;
        remember(line);
        const auto separator = command_line.find_first_of(" \t");
        const auto command = command_line.substr(0, separator);
        const auto arguments = separator == std::string_view::npos ? std::string_view{} : trimmed(command_line.substr(separator));
        auto& registry = client_->registry();
        const auto context = client_->context();
        const auto target = command_target(command);
        // Hidden registrations must still bind through their authority gate.
        const bool registered = registry.registered(target);
        if (builtin(command) && !registered) {
            failure_.reset();
            notice_.clear();
            if (command == "echo") {
                const auto text = echo_text(arguments, arguments.empty() ? line.size() : static_cast<std::size_t>(arguments.data() - line.data()));
                if (!text) {
                    refuse(line, text.error());
                    return false;
                }
                write("] " + std::string{line} + '\n' + *text + '\n');
                return true;
            }
            const auto parsed = Core::parse(line);
            if (!parsed) {
                refuse(line, parsed.error());
                return false;
            }
            const auto count = parsed->tokens.size();
            if ((command == "help" && count > 2) || (command == "find" && count != 2) ||
                ((command == "clear" || command == "quit" || command == "exit") && count != 1)) {
                refuse(line, {Core::Code::Arguments, line.size(), 0});
                return false;
            }
            if (command == "clear") {
                display_.clear();
                display_bytes_ = 0;
                output_trimmed_ = history_trimmed_ = false;
                return true;
            }
            if (command == "help") {
                help(count == 2 ? parsed->tokens[1].text : std::string_view{});
                write("] " + std::string{line} + '\n' + notice_);
                notice_.clear();
                return true;
            }
            if (command == "find") {
                const auto& search = parsed->tokens[1].text;
                std::string result;
                for (const auto& descriptor : commands())
                    if (contains(descriptor.name, search) || contains(descriptor.help, search))
                        append(result, descriptor.name + " - " + display_text(descriptor.help) + '\n', 16384);
                if (result.empty())
                    result = "No matching commands.\n";
                write("] " + std::string{line} + '\n' + result);
                return true;
            }
            refuse(line, {Core::Code::Unsupported});
            return false;
        }
        const auto bind = [&]() -> Core::Checked<Core::BoundRequest> {
            if (command == "echo") {
                const auto descriptor = registry.describe(target, context);
                if (!descriptor)
                    return std::unexpected(descriptor.error());
                if (descriptor->arguments.size() == 1 && descriptor->arguments[0].schema.type == Core::Type::Text &&
                    !arguments.starts_with(descriptor->arguments[0].name + '=')) {
                    const auto text = echo_text(arguments, arguments.empty() ? line.size() : static_cast<std::size_t>(arguments.data() - line.data()));
                    if (!text)
                        return std::unexpected(text.error());
                    return registry.bind(target, {Core::Value::text(*text)}, context);
                }
            }
            if (target == command)
                return Core::bind_text(registry, context, line);
            const auto offset = static_cast<std::size_t>(command.data() - line.data());
            std::string translated{line};
            translated.replace(offset, command.size(), target);
            auto bound = Core::bind_text(registry, context, translated);
            if (!bound)
                return std::unexpected(original_failure(bound.error(), offset, command.size(), target.size()));
            return bound;
        };
        auto bound = bind();
        if (!bound) {
            refuse(line, bound.error());
            return false;
        }
        if (entries_.size() >= Core::max_requests || next_id_ == std::numeric_limits<std::uint64_t>::max()) {
            refuse(line, {Core::Code::Capacity});
            write("Request capacity reached; retained IDs are never evicted. Create a new Model for a fresh session.\n");
            return false;
        }
        entries_.push_back({next_id_, std::string{line}, bound->descriptor().result, {}});
        const auto submitted = client_->session().submit(next_id_, *bound);
        if (!submitted) {
            entries_.pop_back();
            refuse(line, submitted.error());
            return false;
        }
        ++next_id_;
        entries_.back().result = **submitted;
        write(std::string{line}, entries_.back().id);
        failure_.reset();
        notice_.clear();
        return true;
    }

    void Model::refresh() {
        for (auto& entry : entries_) {
            const auto result = client_->session().lookup(entry.id);
            if (result)
                entry.result = **result;
        }
    }
    void Model::pump() {
        if (!closed_) {
            const auto pumped = client_->session().pump();
            if (!pumped)
                refuse({}, pumped.error());
        }
        refresh();
    }
    void Model::close() noexcept {
        if (closed_)
            return;
        closed_ = true;
        client_->close();
    }

    void Model::help(std::string_view command) {
        notice_.clear();
        failure_.reset();
        const auto descriptors = commands();
        for (const auto& descriptor : descriptors) {
            if (!command.empty() && descriptor.name != command)
                continue;
            append(notice_, descriptor.name + " - " + display_text(descriptor.help) + "\n", 16384);
            for (const auto& argument : descriptor.arguments)
                append(notice_, "  " + argument.name + ": " + type_text(argument.schema) + "\n", 16384);
        }
        if (notice_.empty())
            notice_ = "No matching command.\n";
        if (notice_.size() >= 16384 - 64) {
            notice_.resize(prefix_size(notice_, 16384 - 64));
            notice_ += "\n[help truncated at byte limit]\n";
        }
    }

    std::vector<std::string> Model::suggestions(std::string_view line) {
        if (!accepting())
            return {};
        if (const auto failure = input_failure(line)) {
            refuse(line, *failure);
            return {};
        }
        const auto command_line = trimmed(line);
        const auto separator = command_line.find_first_of(" \t");
        const auto command = command_line.substr(0, separator);
        const auto target = command_target(command);
        if (command == "echo" && separator != std::string_view::npos) {
            const auto descriptor = client_->registry().describe(target, client_->context());
            if (!descriptor || (descriptor->arguments.size() == 1 && descriptor->arguments[0].schema.type == Core::Type::Text))
                return {};
        }
        std::string completion_line{line};
        if (target != command && separator != std::string_view::npos)
            completion_line.replace(static_cast<std::size_t>(command.data() - line.data()), command.size(), target);
        auto completion = Core::complete(client_->registry(), client_->context(), completion_line);
        if (!completion) {
            const auto failure = target != command && separator != std::string_view::npos
                ? original_failure(completion.error(), static_cast<std::size_t>(command.data() - line.data()), command.size(), target.size())
                : completion.error();
            refuse(line, failure);
            return {};
        }
        const auto parsed = Core::parse(line, true);
        if (parsed && (parsed->tokens.empty() || (parsed->tokens.size() == 1 && !parsed->trailing_space))) {
            const auto prefix = parsed->tokens.empty() ? std::string_view{} : std::string_view{parsed->tokens.front().text};
            for (const auto& descriptor : commands()) {
                if (descriptor.name.starts_with(prefix))
                    completion->push_back(descriptor.name);
            }
        } else if (parsed && parsed->tokens.front().text == "help" &&
                   !client_->registry().registered("help") &&
                   ((parsed->tokens.size() == 1 && parsed->trailing_space) ||
                    (parsed->tokens.size() == 2 && !parsed->trailing_space))) {
            const auto prefix = parsed->tokens.size() == 1 ? std::string_view{} : std::string_view{parsed->tokens.back().text};
            for (const auto& descriptor : commands())
                if (descriptor.name.starts_with(prefix))
                    completion->push_back(descriptor.name);
        }
        std::ranges::sort(*completion);
        completion->erase(std::unique(completion->begin(), completion->end()), completion->end());
        std::vector<std::string> result;
        std::size_t bytes = 0;
        for (const auto& candidate : *completion) {
            if (result.size() == suggestion_count || bytes + candidate.size() > suggestion_bytes)
                break;
            bytes += candidate.size();
            result.push_back(candidate);
        }
        return result;
    }

    std::string Model::complete(std::string_view line) {
        const auto candidates = suggestions(line);
        if (candidates.empty())
            return std::string{line.substr(0, prefix_size(line, command_bytes))};
        return complete(line, candidates.front());
    }

    std::string Model::complete(std::string_view line, std::string_view candidate) {
        const auto candidates = suggestions(line);
        const auto parsed = Core::parse(line, true);
        if (!parsed || std::ranges::find(candidates, candidate) == candidates.end())
            return std::string{line.substr(0, prefix_size(line, command_bytes))};
        std::string completed{line};
        if (!parsed->tokens.empty() && !parsed->trailing_space) {
            const auto& token = parsed->tokens.back();
            completed.resize(token.offset);
            if (token.equals)
                completed += token.text.substr(0, *token.equals + 1);
        }
        completed += quote(candidate);
        if (!completed.ends_with('='))
            completed += ' ';
        if (completed.size() > command_bytes) {
            refuse(line, {Core::Code::Limit});
            return std::string{line};
        }
        return completed;
    }

    std::string Model::previous(std::string_view current) {
        if (history_cursor_ == history_.size())
            draft_ = std::string{current.substr(0, prefix_size(current, command_bytes))};
        if (history_cursor_)
            --history_cursor_;
        return history_cursor_ < history_.size() ? history_[history_cursor_] : draft_;
    }
    std::string Model::next() {
        if (history_cursor_ < history_.size())
            ++history_cursor_;
        return history_cursor_ < history_.size() ? history_[history_cursor_] : draft_;
    }

    std::string Model::text() const {
        constexpr std::string_view omitted = "[output truncated: showing recent entries; retained typed results remain owned]\n\n";
        constexpr std::string_view value_omitted = "\n[value truncated at byte limit; retained typed result remains owned]";
        std::string history;
        if (history_trimmed_)
            history = "[history truncated: oldest entries discarded at the count/byte limit]\n\n";
        const std::size_t available = output_bytes - history.size() - notice_.size() - omitted.size();
        std::vector<std::string> recent;
        std::size_t used = 0;
        for (auto iterator = display_.rbegin(); iterator != display_.rend(); ++iterator) {
            auto rendered = iterator->text;
            if (iterator->request) {
                const auto& entry = entries_[iterator->request - 1];
                rendered = "] " + display_text(entry.line) + '\n';
                if (entry.result.stage != Core::Stage::Succeeded)
                    rendered += std::format("[{}]\n", stage_name(entry.result.stage));
                if (entry.result.failure.code != Core::Code::None)
                    rendered += std::format("Error: {} at UTF-8 bytes [{}, {})\n", code_name(entry.result.failure.code),
                                            entry.result.failure.offset, entry.result.failure.offset + entry.result.failure.length);
                if (entry.result.stage != Core::Stage::Queued) {
                    std::string value;
                    if (!value_text(value, entry.result.value, entry.schema, Core::max_output)) {
                        value.resize(prefix_size(value, Core::max_output - value_omitted.size()));
                        value += value_omitted;
                    }
                    rendered += value + "\n";
                }
            }
            if (rendered.size() > available - used)
                break;
            used += rendered.size();
            recent.push_back(std::move(rendered));
        }
        std::string output;
        output.reserve(history.size() + omitted.size() + used + notice_.size());
        output += history;
        if (output_trimmed_ || recent.size() != display_.size())
            output += omitted;
        for (auto iterator = recent.rbegin(); iterator != recent.rend(); ++iterator)
            output += *iterator;
        output += notice_;
        return output;
    }

    std::string display_text(std::string_view text) {
        if (!Core::valid_utf8(text))
            return "[invalid UTF-8]";
        std::string result;
        for (std::size_t offset = 0; offset < text.size();) {
            const auto [codepoint, length] = scalar(text, offset);
            if (unsafe(codepoint))
                result += "[control]";
            else
                result.append(text.substr(offset, length));
            offset += length;
        }
        return result;
    }
    std::string escaped(std::string_view text) {
        std::string result;
        for (const auto character : text) {
            switch (character) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += character;
            }
        }
        return result;
    }
    std::string_view stage_name(Core::Stage stage) noexcept {
        switch (stage) {
        case Core::Stage::Queued: return "queued";
        case Core::Stage::Pending: return "pending";
        case Core::Stage::Succeeded: return "succeeded";
        case Core::Stage::Refused: return "refused";
        case Core::Stage::Failed: return "failed";
        case Core::Stage::Cancelled: return "cancelled";
        }
        return "unknown";
    }
    std::string_view code_name(Core::Code code) noexcept {
        switch (code) {
        case Core::Code::None: return "none";
        case Core::Code::Unauthorized: return "unauthorized";
        case Core::Code::Expired: return "expired/closed";
        case Core::Code::UnknownCommand: return "unknown command";
        case Core::Code::Duplicate: return "duplicate";
        case Core::Code::Schema: return "schema";
        case Core::Code::Unsupported: return "unsupported";
        case Core::Code::Syntax: return "syntax";
        case Core::Code::Encoding: return "encoding/control character";
        case Core::Code::Limit: return "byte/token limit";
        case Core::Code::Type: return "type";
        case Core::Code::Range: return "range";
        case Core::Code::Arguments: return "arguments";
        case Core::Code::Conflict: return "conflict";
        case Core::Code::Unregistered: return "unregistered";
        case Core::Code::Capacity: return "capacity";
        case Core::Code::Allocation: return "allocation";
        case Core::Code::Owner: return "owner failure";
        case Core::Code::Cancelled: return "cancelled";
        case Core::Code::Reentrant: return "reentrant";
        }
        return "unknown";
    }
}
