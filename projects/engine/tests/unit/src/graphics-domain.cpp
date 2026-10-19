#include <core/rendering/graphics-domain.h>

#include <gtest/gtest.h>

#include <future>

namespace {
    using CE::Rendering::GraphicsDomain;
    using CE::Rendering::GraphicsDomainOwner;
    using CE::Rendering::GraphicsDomainReason;
    using CE::Rendering::GraphicsDomainSnapshot;
    using CE::Rendering::GraphicsDomainStatus;
}

TEST(graphics_domain, retained_invalidation) {
    GraphicsDomain retained;
    EXPECT_EQ(retained.snapshot(), GraphicsDomainSnapshot{});
    {
        GraphicsDomainOwner owner;
        retained = owner.token();
        const auto before = retained.snapshot();
        EXPECT_NE(before.id, 0u);
        EXPECT_EQ(before.sequence, 0u);
        owner.make_available();
        EXPECT_EQ(
            retained.snapshot(), (GraphicsDomainSnapshot{before.id, 1, GraphicsDomainStatus::Available, GraphicsDomainReason::Initialized})
        );
    }
    const auto terminal = retained.snapshot();
    EXPECT_EQ(terminal.sequence, 2u);
    EXPECT_EQ(terminal.status, GraphicsDomainStatus::Unavailable);
    EXPECT_EQ(terminal.reason, GraphicsDomainReason::Abandoned);
    EXPECT_EQ(std::async(std::launch::async, [retained] { return retained.snapshot(); }).get(), terminal);
    GraphicsDomainOwner replacement;
    EXPECT_NE(replacement.token().snapshot().id, terminal.id);
}

TEST(graphics_domain, permanent_shutdown) {
    GraphicsDomainOwner owner;
    const auto domain = owner.token();
    owner.make_available();
    owner.shutdown();
    const auto stopped = domain.snapshot();
    EXPECT_EQ(stopped.reason, GraphicsDomainReason::Shutdown);
    owner.abandon();
    owner.shutdown();
    owner.make_available();
    EXPECT_EQ(domain.snapshot(), stopped);
}

TEST(graphics_domain, uninitialized_domain) {
    GraphicsDomain retained;
    {
        GraphicsDomainOwner owner;
        retained = owner.token();
        owner.shutdown();
        owner.abandon();
        EXPECT_EQ(retained.snapshot().sequence, 0u);
    }
    EXPECT_EQ(retained.snapshot().sequence, 0u);
    EXPECT_EQ(retained.snapshot().reason, GraphicsDomainReason::NotInitialized);
}
