#include <core/engine/view-state.h>
#include "core/engine/view-state-internal.h"
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    using namespace CE::Runtime;
    using Access = Detail::ViewSourceAccess;

    struct Views {
        CE::Events::EventBus bus;
        WindowStateSource window = Access::window_source(bus);
        SimulationViewSource simulation = Access::simulation_source(bus);
        Views() {
            Access::reserve(window, Access::identity(), 42);
            Access::reserve(simulation, Access::identity(), 42);
        }
        ~Views() {
            Access::close(window);
            Access::close(simulation);
        }
        bool select(int logical = 100, int pixels = 200, std::optional<bool> focus = {}) {
            WindowState state;
            state.logical_size = {logical, logical};
            state.framebuffer_size = {pixels, pixels};
            state.focused = focus;
            return Access::select(window, state);
        }
    };
}

TEST(view_state, seed_and_changes) {
    Views views;
    EXPECT_THROW(static_cast<void>(views.window.watch([](const auto&) {})), CE::Exceptions::failed_operation);
    ASSERT_TRUE(views.select());
    std::vector<WindowStateChanged> seen;
    auto watch = views.window.watch([&](const auto& change) { seen.push_back(change); });
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_TRUE(seen[0].seed);
    EXPECT_EQ(seen[0].current.session, 42u);
    EXPECT_FALSE(seen[0].current.focused.has_value());
    EXPECT_FALSE(views.select());
    EXPECT_TRUE(views.select(100, 200, true));
    EXPECT_TRUE(views.select(0, 0, false));
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[1].previous.revision, seen[0].current.revision);
    EXPECT_EQ(seen[2].current.revision, 3u);
    EXPECT_EQ(seen[2].current.framebuffer_size.width, 0);
    watch.reset();
    EXPECT_TRUE(views.select(5, 10));
    EXPECT_EQ(seen.size(), 3u);
}

TEST(view_state, fallback_and_partial_failure) {
    Views views;
    views.select();
    int first = 0;
    int later = 0;
    auto throwing = views.window.watch([&](const auto& change) {
        ++first;
        if (!change.seed)
            throw std::runtime_error("partial sizing");
    });
    auto succeeding = views.window.watch([&](const auto&) { ++later; });
    Access::before_notify(views.window, [] { throw std::runtime_error("before bus snapshot"); });
    EXPECT_THROW(views.select(101), std::runtime_error);
    EXPECT_EQ(first, 2); // One seed and one entered failing action; never a retry.
    EXPECT_EQ(later, 2);
    EXPECT_EQ(views.window.snapshot().logical_size.width, 101);
    Access::before_notify(views.window, {});
    views.bus.register_listener(window_state_changed_event, [](const auto&) { throw std::runtime_error("optional"); });
    EXPECT_THROW(views.select(102), std::runtime_error);
    EXPECT_EQ(first, 3);
    EXPECT_EQ(later, 3);
}

TEST(view_state, failed_seed_rolls_back) {
    Views views;
    views.select();
    int calls = 0;
    EXPECT_THROW(
        static_cast<void>(views.window.watch([&](const auto&) {
            ++calls;
            throw std::runtime_error("initial sizing");
        })),
        std::runtime_error
    );
    EXPECT_TRUE(views.select(101));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(views.bus.diagnostics().active, 0u);
}

TEST(view_state, detach_and_late_seed) {
    Views views;
    views.select();
    ViewAttachment removed;
    ViewAttachment added;
    std::vector<bool> added_seeds;
    int removed_calls = 0;
    views.bus.register_listener(window_state_changed_event, [&](const auto&) {
        removed.reset(); // Must also suppress direct fallback.
        if (!added.attached())
            added = views.window.watch([&](const auto& change) { added_seeds.push_back(change.seed); });
    });
    removed = views.window.watch([&](const auto&) { ++removed_calls; });
    views.select(101);
    EXPECT_EQ(removed_calls, 1);
    ASSERT_EQ(added_seeds, (std::vector<bool>{true}));
    views.select(102);
    EXPECT_EQ(added_seeds, (std::vector<bool>{true, false}));
}

TEST(view_state, self_detach_and_forgery) {
    Views views;
    views.select();
    ViewAttachment watch;
    int calls = 0;
    watch = views.window.watch([&](const auto& change) {
        ++calls;
        if (!change.seed)
            watch.reset();
    });
    auto current = views.window.snapshot();
    views.bus.notify(window_state_changed_event, WindowStateChanged{current, current});
    EXPECT_EQ(calls, 1);
    views.select(101);
    EXPECT_FALSE(watch.attached());
    views.select(102);
    EXPECT_EQ(calls, 2);
}

TEST(view_state, owner_and_closure) {
    Views views;
    views.select();
    auto result = std::async(std::launch::async, [&] {
        EXPECT_THROW(static_cast<void>(views.window.watch([](const auto&) {})), CE::Exceptions::failed_operation);
        EXPECT_TRUE(views.window.snapshot().initialized);
    });
    result.get();
    int calls = 0;
    auto attachment = views.window.watch([&](const auto&) { ++calls; });
    Access::close(views.window);
    EXPECT_TRUE(views.window.snapshot().closed);
    EXPECT_FALSE(attachment.attached());
    EXPECT_EQ(calls, 1);
    views.bus.notify(window_state_changed_event, WindowStateChanged{});
    EXPECT_EQ(calls, 1);
}

TEST(view_state, projection_ignores_focus) {
    Views views;
    views.select();
    SimulationView next;
    next.logical_size = {100, 100};
    next.framebuffer_size = {200, 200};
    next.window_revision = 1;
    EXPECT_TRUE(Access::select(views.simulation, next));
    next.window_revision = 2;
    EXPECT_FALSE(Access::select(views.simulation, next));
    EXPECT_EQ(views.simulation.snapshot().revision, 1u);
    next.logical_size = {50, 50};
    EXPECT_TRUE(Access::select(views.simulation, next));
    EXPECT_EQ(views.simulation.snapshot().window_revision, 2u);
}

TEST(view_state, closed_bus_fallback) {
    Views views;
    views.select();
    int calls = 0;
    auto attachment = views.window.watch([&](const auto&) { ++calls; });
    views.bus.close();
    EXPECT_TRUE(views.select(101));
    EXPECT_EQ(calls, 2);
}

TEST(view_state, detach_waits_for_entry) {
    Views views;
    std::promise<ViewAttachment> installed;
    std::promise<void> select;
    std::promise<void> entered;
    std::promise<void> release;
    auto selection = select.get_future();
    auto released = release.get_future();
    std::thread owner([&] {
        views.select();
        installed.set_value(views.window.watch([&](const auto& change) {
            if (!change.seed) {
                entered.set_value();
                released.wait();
            }
        }));
        selection.wait();
        views.select(101);
    });
    auto attachment = installed.get_future().get();
    select.set_value();
    entered.get_future().wait();
    std::promise<void> detaching;
    auto detached = std::async(std::launch::async, [&] {
        detaching.set_value();
        attachment.reset();
    });
    detaching.get_future().wait();
    EXPECT_EQ(detached.wait_for(std::chrono::milliseconds{0}), std::future_status::timeout);
    release.set_value();
    detached.get();
    owner.join();
    EXPECT_FALSE(attachment.attached());
}
