#include <magic/compiler.h>
#include "literals.h"

#include <iostream>
#include <string>
#include <type_traits>

static_assert(!std::is_default_constructible_v<Magic::Plan>);
static_assert(!std::is_copy_constructible_v<Magic::Plan>);
static_assert(std::is_same_v<decltype(std::declval<Magic::PlanHandle>()->meaning()), const Magic::Meaning&>);

int main() {
    using namespace Magic;
    int passed = 0;
    // Ordinary caller 1: a local edit session adopts success, preserves it after failure,
    // then inspects it after its text, catalog and result are destroyed.
    PlanHandle accepted;
    Diagnostic last_error{};
    {
        Catalog host;
        host.operations = {{Primitive::Heal, "actor.restore"}};
        std::string draft = "spoken release via contact after 0 ms { bind friend = willing touched actor at release ; "
                            "together ordered { on friend bridge Mend@1 { sela 19 hp tava-a } ; } }";
        auto result = compile(draft, host);
        if (!result)
            return 1;
        accepted = *result;
        draft = "spoken release via contact after 0 ms { bind friend = willing touched actor at release ; "
                "together ordered { on friend bridge Mend@1 { sela -1 hp tava-a } ; } }";
        auto revision = compile(draft, host);
        if (revision)
            accepted = *revision;
        else
            last_error = revision.error();
        host.operations.clear();
    }
    if (!accepted || accepted->meaning().effects.size() != 1 || accepted->meaning().effects[0].quantity != 19 ||
        accepted->meaning().effects[0].key != "actor.restore" || accepted->common().find("heal 19 hp instant") == std::string_view::npos ||
        last_error.code != Code::Range || last_error.excerpt != "-1")
        return 1;
    std::cout << "PASS caller edit/adopt/refuse/retain\n";
    ++passed;

    // Ordinary caller 2: independently authored bilingual lesson, then a new amount/duration.
    Catalog host;
    host.operations = {{Primitive::Heal, "actor.restore"}, {Primitive::Slow, "actor.speed"}, {Primitive::Fire, "actor.burn"}};
    auto lesson = compile(
        "spoken release via burst after 1 s { "
        "bind crowd = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; "
        "together ordered { on crowd bridge Cinder@1 { 8 dp hara } ; "
        "on crowd bridge Mend@1 { luma 750 permille tava-a lasting 2 s } ; } }",
        host
    );
    auto invention = compile(
        "spoken release via burst after 1 s { "
        "bind crowd = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; "
        "together ordered { on crowd bridge Mend@1 { sela 23 hp tava-a } ; "
        "on crowd bridge Cinder@1 { 3 s 650 permille neth } ; } }",
        host
    );
    if (!lesson || !invention || (*lesson)->meaning().onset_ticks != 100 || (*invention)->meaning().effects[0].quantity != 23 ||
        (*invention)->meaning().effects[1].quantity != 650 || (*invention)->meaning().effects[1].duration_ticks != 300 ||
        (*invention)->meaning().effects[1].binding != 0)
        return 1;
    std::cout << "PASS caller lesson/new-composition\n";
    ++passed;

    // Ordinary caller 3: the same compiled payload is retained under an event entry.
    auto inscription = compile(
        "engraved item-touch charges 1 release via item-event after 0 ms { "
        "bind guest = willing event toucher at activation ; "
        "together ordered { on guest bridge Common@1 { heal 19 hp instant } ; } }",
        host
    );
    if (!inscription || (*inscription)->meaning().entry.kind != EntryKind::Engraved || (*inscription)->meaning().entry.charges != 1 ||
        (*inscription)->meaning().entry.mode != CastingMode::Release || (*inscription)->meaning().entry.trigger != Trigger::ItemTouch ||
        (*inscription)->meaning().bindings[0].query != Query::WillingEventToucher ||
        (*inscription)->inspection().find("metadata only") == std::string_view::npos)
        return 1;
    std::cout << "PASS caller engraved-inspect-only\n";
    ++passed;

    // Ordinary caller 4: no world scale is silently inferred; context is an explicit argument.
    const std::string px = "spoken release via burst after 0 ms { "
                           "bind crowd = eligible actors in G0 disk 25 px around aim captured at release queried at activation inclusive ; "
                           "together ordered { on crowd bridge Common@1 { multiply speed by 750 permille for 2 s } ; } }";
    auto unqualified = compile(px, host);
    Options context;
    context.pixels = PixelContext{"G0", "C7", 3, "G0", 10, 1, true};
    auto qualified = compile(px, host, context);
    if (unqualified || unqualified.error().code != Code::MissingContext || !qualified ||
        (*qualified)->meaning().bindings[0].radius_mm != 250 || (*qualified)->meaning().effects[0].duration_ticks != 200)
        return 1;
    auto common = compile((*qualified)->common(), host);
    if (!common || (*common)->meaning() != (*qualified)->meaning())
        return 1;
    std::cout << "PASS caller contextual-units/Common-roundtrip\n";
    ++passed;
    std::cout << passed << " ordinary caller scenarios passed.\n";
    return 0;
}
