#include <cheryl/tools/console/parser.h>
#include <cheryl/tools/console/session.h>

#include <cstdlib>
#include <new>

namespace { bool reject_allocations = false; }
void* operator new(std::size_t size) {
    if (reject_allocations) throw std::bad_alloc();
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
    namespace Core = CE::Tools::Console;
    class Apply final : public Core::Work {
        unsigned& effects_;
    public:
        explicit Apply(unsigned& effects) : effects_(effects) { result.value = Core::Value::unsigned_integer(0); }
        void step() noexcept override {
            ++effects_; result.value = Core::Value::unsigned_integer(effects_); result.stage = Core::Stage::Succeeded;
        }
        void close() noexcept override { result.stage = Core::Stage::Cancelled; }
    };
}
int main() {
    try {
        Core::Authority authority; auto context = authority.issue(0); Core::Registry registry; Core::Session session(context);
        unsigned effects = 0;
        Core::Schema text, output; output.type = Core::Type::Unsigned;
        Core::Descriptor descriptor{"apply", "Allocation boundary.", "test", Core::Kind::Action, Core::Timing::OwnerImmediate,
            0, {{"text", text}}, output};
        auto registration = registry.add<std::string>(descriptor, [&effects](const Core::Context&, std::string) {
            return std::make_shared<Apply>(effects);
        });
        if (!registration) return 1;
        auto request = registry.bind("apply", {Core::Value::text(std::string(128, 'x'))}, context);
        if (!request) return 2;
        reject_allocations = true;
        auto failed = session.submit(1, *request);
        reject_allocations = false;
        if (failed || failed.error().code != Core::Code::Allocation || session.retained() || effects) return 3;
        if (!session.submit(1, *request)) return 4;
        reject_allocations = true;
        auto pumped = session.pump(); auto retained = session.lookup(1); auto retry = session.submit(1, *request);
        reject_allocations = false;
        if (!pumped || !retained || !retry || (*retained)->stage != Core::Stage::Failed ||
            (*retained)->failure.code != Core::Code::Allocation || effects) return 5;
        if (!session.submit(2, *request) || !session.pump() || effects != 1) return 6;
        reject_allocations = true;
        retained = session.lookup(2); retry = session.submit(2, *request); session.close();
        reject_allocations = false;
        if (!retained || !retry || (*retained)->stage != Core::Stage::Succeeded || effects != 1) return 7;
        return 0;
    } catch (...) { reject_allocations = false; return 99; }
}
