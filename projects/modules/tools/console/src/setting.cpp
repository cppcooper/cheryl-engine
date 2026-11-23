#include <cheryl/tools/console/setting.h>

#include <optional>
#include <stdexcept>

namespace CE::Tools::Console {
    namespace {
        Schema number(std::uint64_t maximum) {
            Schema schema;
            schema.type = Type::Unsigned;
            schema.unsigned_max = maximum;
            return schema;
        }
        class SettingWork final : public Work {
            UnsignedSettingOwner owner_;
            std::optional<std::uint64_t> proposed_;
        public:
            SettingWork(UnsignedSettingOwner owner, std::optional<std::uint64_t> proposed,
                std::uint64_t default_value, std::uint64_t maximum)
                : owner_(std::move(owner)), proposed_(proposed) {
                result.value = Value::record({Value::unsigned_integer(default_value), Value::unsigned_integer(default_value),
                    Value::unsigned_integer(0), Value::unsigned_integer(maximum), Value::enumeration(0)});
            }
            void step() noexcept override {
                if (terminal(result.stage)) return;
                try {
                    const auto applied = !proposed_ || owner_.write(*proposed_);
                    auto& fields = std::get<Value::Children>(result.value.data);
                    fields[1].data = owner_.read();
                    result.stage = applied ? Stage::Succeeded : Stage::Refused;
                    if (!applied) result.failure = {Code::Owner};
                } catch (...) { result.stage = Stage::Failed; result.failure = {Code::Owner}; }
            }
            void close() noexcept override {
                if (!terminal(result.stage)) { result.stage = Stage::Cancelled; result.failure = {Code::Cancelled}; }
            }
        };
    }
    UnsignedSetting::UnsignedSetting(Registry& registry, std::string name, std::string help,
        std::uint64_t default_value, std::uint64_t maximum, UnsignedSettingOwner owner,
        Rights read_rights, Rights write_rights) {
        if (default_value > maximum || !owner.read || !owner.write)
            throw std::invalid_argument("Unsigned setting requires valid bounds and owner callbacks");
        Schema timing;
        timing.type = Type::Enumeration; timing.choices = {"OwnerImmediate"};
        Schema output;
        output.type = Type::Record;
        output.fields = {"default", "effective", "minimum", "maximum", "timing"};
        const auto value = number(maximum);
        output.children = {value, value, value, value, timing};
        Descriptor descriptor;
        descriptor.name = name + ".read"; descriptor.help = help;
        descriptor.domain = "setting"; descriptor.rights = read_rights; descriptor.result = output;
        auto installed = registry.add<>(descriptor, [owner, default_value, maximum](const Context&) {
            return std::make_shared<SettingWork>(owner, std::nullopt, default_value, maximum);
        });
        if (!installed) throw std::runtime_error("Cannot register setting reader");
        read_ = std::move(*installed);
        descriptor.name = name + ".set"; descriptor.kind = Kind::Setting; descriptor.rights = write_rights;
        descriptor.arguments = {{"value", value}};
        installed = registry.add<std::uint64_t>(std::move(descriptor),
            [owner, default_value, maximum](const Context&, std::uint64_t proposed) {
                return std::make_shared<SettingWork>(owner, proposed, default_value, maximum);
            });
        if (!installed) throw std::runtime_error("Cannot register setting writer");
        write_ = std::move(*installed);
    }
}
