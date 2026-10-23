#include "../src/settings-apply.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace {
    using namespace Tutorial;
    using namespace CE::Input;
    namespace fs = std::filesystem;
    using Key = KeyboardKey;
    using Status = SettingsApply::Status;
    constexpr DeviceId keyboard = 17;
    const GameSettings changed{200, false, Key::B};
    const std::string predecessor = R"({"version":1,"camera_pan_speed":100,"show_health":true,"bindings":{"add_score":"space"}})";

    class Files {
        fs::path root_;

    public:
        Files() {
            const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
            for (unsigned int attempt = 0;; ++attempt) {
                root_ = fs::absolute("tutorial-apply-" + std::to_string(seed) + "-" + std::to_string(attempt));
                if (fs::create_directory(root_))
                    break;
            }
        }
        ~Files() {
            std::error_code error;
            static_cast<void>(fs::remove_all(root_, error));
        }
        fs::path path() const { return root_ / "settings.json"; }
        fs::path pending() const { return root_ / "settings.json.pending"; }
        static void write(const fs::path& path, std::string_view bytes) {
            std::ofstream output(path, std::ios::binary);
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.close();
        }
        static std::string read(const fs::path& path) {
            std::ifstream input(path, std::ios::binary);
            if (!input)
                throw std::runtime_error("Fixture cannot read " + path.string());
            return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        }
    };

    enum class Fault { None, Open, Write, Flush, Close, Rename, Allocation, Programming };

    // Uses U1's private operation seam and real disposable streams/rename. No
    // separate storage protocol or production callback/injector is introduced.
    class Operations final : public Detail::SettingsFileOperations {
    public:
        Fault fault = Fault::None;
        bool cleanup_fails = false, cleaned_owned = false;
        unsigned int opens = 0, writes = 0, flushes = 0, closes = 0, renames = 0, commits = 0, cleanups = 0;
        fs::path owned;
        const GameSettings* active = nullptr;
        const ScoreInput* score = nullptr;
        Actions::ContextId original = 0;

        static void fail() { throw std::system_error(std::make_error_code(std::errc::no_space_on_device), "injected Apply I/O"); }
        std::FILE* open_exclusive(const fs::path& path) override {
            ++opens;
            if (fault == Fault::Open)
                fail();
            owned = path;
            return SettingsFileOperations::open_exclusive(path);
        }
        void write(std::FILE* file, std::string_view bytes) override {
            ++writes;
            if (fault == Fault::Allocation)
                throw std::bad_alloc();
            if (fault == Fault::Programming)
                throw std::logic_error("injected Apply programming failure");
            if (fault == Fault::Write) {
                SettingsFileOperations::write(file, bytes.substr(0, bytes.size() / 2));
                fail();
            }
            SettingsFileOperations::write(file, bytes);
        }
        void flush(std::FILE* file) override {
            ++flushes;
            if (fault == Fault::Flush)
                fail();
            SettingsFileOperations::flush(file);
        }
        void close(std::FILE* file) override {
            ++closes;
            SettingsFileOperations::close(file);
            if (fault == Fault::Close)
                fail();
        }
        void replace(const fs::path& temporary, const fs::path& target) override {
            ++renames;
            // Last pre-commit observation: neither active object has changed.
            if (active && score) {
                EXPECT_EQ(*active, GameSettings{});
                EXPECT_EQ(score->key(), Key::Space);
                EXPECT_EQ(score->active_context(), original);
                EXPECT_EQ(Files::read(target), predecessor);
            }
            if (fault == Fault::Rename)
                fail();
            SettingsFileOperations::replace(temporary, target);
            ++commits; // Primitive bookkeeping only after rename; cannot throw.
        }
        void remove(const fs::path& path, std::error_code& error) noexcept override {
            ++cleanups;
            cleaned_owned = path == owned;
            if (cleanup_fails)
                error = std::make_error_code(std::errc::permission_denied);
            else
                SettingsFileOperations::remove(path, error);
        }
    };

    struct Polls {
        Actions::Bindings mapper;
        std::shared_ptr<const Actions::Snapshot> previous = mapper.action_snapshot();

        Tick poll() {
            auto next = mapper.publish_actions();
            Tick tick(previous, {next});
            previous = std::move(next);
            return tick;
        }
        Tick idle() const { return Tick(previous, {}); }
        void key(Key key, bool held) { mapper.on_button({keyboard, key}, held); }
    };

    struct ApplyCase {
        Files files;
        Operations operations;
        SettingsFile file{files.path(), operations};
        Polls input;
        GameSettings active;
        ScoreInput score{input.mapper.action_contexts(), keyboard};
        SettingsApply apply{active, score, file};
        Actions::ContextId original = score.active_context();

        ApplyCase() {
            Files::write(files.path(), predecessor);
            score.observe(input.poll());
            operations.active = &active;
            operations.score = &score;
            operations.original = original;
        }
        SettingsApply::Generation ready() {
            apply.edit(changed);
            const auto request = apply.request();
            EXPECT_EQ(apply.observe(request, input.poll()), Status::Ready);
            return request;
        }
        void preserved() {
            EXPECT_EQ(Files::read(files.path()), predecessor);
            EXPECT_EQ(active, GameSettings{});
            EXPECT_EQ(score.key(), Key::Space);
            EXPECT_EQ(score.active_context(), original);
            EXPECT_EQ(apply.draft(), changed);
            EXPECT_EQ(operations.commits, 0u);
        }
        void applied() {
            EXPECT_EQ(active, changed);
            EXPECT_EQ(score.key(), Key::B);
            EXPECT_NE(score.active_context(), original);
            const auto saved = file.load_or_defaults();
            EXPECT_EQ(saved.status, SettingsLoadStatus::Loaded);
            EXPECT_EQ(saved.value, changed);
            EXPECT_NE(Files::read(files.path()), predecessor);
            EXPECT_FALSE(fs::exists(files.pending()));
            EXPECT_EQ(operations.commits, 1u);
        }
    };

    void failure(Fault fault, std::string_view operation) {
        ApplyCase test;
        const auto request = test.ready();
        const auto abandoned = test.apply.pending_context();
        ASSERT_NE(abandoned, 0u);
        test.operations.fault = fault;
        try {
            static_cast<void>(test.apply.commit(request));
            FAIL() << "Expected the injected " << operation << " failure";
        } catch (const SettingsFileError& error) {
            EXPECT_NE(std::string(error.what()).find(operation), std::string::npos) << error.what();
            EXPECT_NE(std::string(error.what()).find(test.files.path().string()), std::string::npos) << error.what();
        }
        test.preserved();
        EXPECT_EQ(test.apply.status(request), Status::Failed);
        EXPECT_EQ(test.apply.pending_context(), 0u);
        EXPECT_FALSE(fs::exists(test.files.pending()));
        EXPECT_EQ(test.operations.opens, 1u);
        EXPECT_EQ(test.operations.cleanups, fault == Fault::Open ? 0u : 1u);
        EXPECT_EQ(test.operations.closes, fault == Fault::Open ? 0u : 1u);
        EXPECT_EQ(test.operations.cleaned_owned, fault != Fault::Open);
        EXPECT_EQ(test.apply.commit(request), Status::Failed); // No implicit retry.
        EXPECT_EQ(test.operations.opens, 1u);
        const auto release = test.input.poll();
        EXPECT_TRUE(release.latest_poll()->has_context(abandoned));
        EXPECT_TRUE(release.latest_poll()->has_context(test.original));
        EXPECT_FALSE(test.input.poll().latest_poll()->has_context(abandoned));

        test.operations.fault = Fault::None;
        const auto retry = test.apply.request(); // Draft retained; explicit new attempt.
        EXPECT_NE(retry, request);
        const auto replacement = test.apply.pending_context();
        EXPECT_NE(replacement, abandoned);
        EXPECT_EQ(test.apply.commit(request), Status::Stale);
        EXPECT_EQ(test.apply.cancel(request), Status::Stale);
        EXPECT_EQ(test.operations.opens, 1u);
        EXPECT_EQ(test.apply.observe(retry, test.input.poll()), Status::Ready);
        ASSERT_EQ(test.apply.commit(retry), Status::Applied);
        test.applied();
        EXPECT_EQ(test.score.active_context(), replacement);
        EXPECT_EQ(test.apply.commit(retry), Status::Applied);
        EXPECT_EQ(test.operations.opens, 2u);
        EXPECT_EQ(test.operations.commits, 1u);
    }

    static_assert(!std::is_copy_constructible_v<SettingsApply> && !std::is_move_constructible_v<SettingsApply>);
    static_assert(std::is_nothrow_destructible_v<SettingsApply>);
    static_assert(std::is_trivially_copyable_v<GameSettings> && std::is_nothrow_copy_assignable_v<GameSettings>);
    static_assert(noexcept(std::declval<SettingsApply&>().cancel(1)));
    static_assert(noexcept(std::declval<SettingsApply&>().observe(1, std::declval<const Tick&>())));
}

TEST(tutorial_settings_apply, changed) {
    ApplyCase test;
    test.input.key(Key::Space, true);
    test.score.observe(test.input.poll());
    const auto request = test.ready();
    const auto replacement = test.apply.pending_context();
    test.preserved();
    ASSERT_EQ(test.apply.commit(request), Status::Applied);
    test.applied();
    EXPECT_EQ(test.score.active_context(), replacement);
    EXPECT_EQ(test.apply.pending_context(), 0u);
    const auto departing = test.input.poll();
    EXPECT_TRUE(departing.latest_poll()->has_context(test.original));
    EXPECT_TRUE(departing.actions(test.original).button(Actions::Id{1}).released());
    EXPECT_TRUE(departing.latest_poll()->has_context(replacement));
    const auto retired = test.input.poll();
    EXPECT_FALSE(retired.latest_poll()->has_context(test.original));
    EXPECT_TRUE(retired.latest_poll()->has_context(replacement));
    EXPECT_EQ(test.apply.commit(request), Status::Applied);
    EXPECT_EQ(test.apply.cancel(request), Status::Applied);
    EXPECT_THROW(static_cast<void>(test.apply.request()), std::logic_error);
    EXPECT_EQ(test.operations.opens, 1u);
    EXPECT_EQ(test.operations.commits, 1u);
}

TEST(tutorial_settings_apply, unchanged) {
    ApplyCase test;
    const GameSettings draft{300, false, Key::Space};
    test.apply.edit(draft);
    const auto request = test.apply.request();
    EXPECT_EQ(test.apply.pending_context(), 0u);
    EXPECT_EQ(test.apply.status(request), Status::Ready); // Already witnessed active lease.
    ASSERT_EQ(test.apply.commit(request), Status::Applied);
    EXPECT_EQ(test.active, draft);
    EXPECT_EQ(test.score.active_context(), test.original);
    EXPECT_EQ(test.score.key(), Key::Space);
    EXPECT_EQ(test.file.load_or_defaults().value, draft);
    EXPECT_EQ(test.operations.commits, 1u);
    EXPECT_EQ(test.apply.commit(request), Status::Applied);
    EXPECT_EQ(test.operations.opens, 1u);
}

TEST(tutorial_settings_apply, unchanged_failure) {
    ApplyCase test;
    const GameSettings draft{250, false, Key::Space};
    test.apply.edit(draft);
    const auto request = test.apply.request();
    test.operations.fault = Fault::Rename;
    EXPECT_THROW(static_cast<void>(test.apply.commit(request)), SettingsFileError);
    EXPECT_EQ(test.apply.status(request), Status::Failed);
    EXPECT_EQ(test.apply.draft(), draft);
    EXPECT_EQ(test.active, GameSettings{});
    EXPECT_EQ(test.score.active_context(), test.original);
    EXPECT_EQ(test.score.key(), Key::Space);
    EXPECT_EQ(Files::read(test.files.path()), predecessor);
    EXPECT_EQ(test.operations.commits, 0u);
    EXPECT_FALSE(fs::exists(test.files.pending()));
    test.operations.fault = Fault::None;
    const auto retry = test.apply.request();
    EXPECT_NE(retry, request);
    EXPECT_EQ(test.apply.pending_context(), 0u);
    EXPECT_EQ(test.apply.commit(retry), Status::Applied);
    EXPECT_EQ(test.active, draft);
    EXPECT_EQ(test.file.load_or_defaults().value, draft);
    EXPECT_EQ(test.score.active_context(), test.original);
    EXPECT_EQ(test.operations.commits, 1u);
}

TEST(tutorial_settings_apply, initial_pending) {
    Files files;
    Operations operations;
    SettingsFile file(files.path(), operations);
    Polls input;
    GameSettings active;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    SettingsApply apply(active, score, file);
    const auto original = score.active_context();
    apply.edit({250, false, Key::Space});
    const auto request = apply.request();
    EXPECT_EQ(apply.observe(request, input.idle()), Status::Pending);
    EXPECT_EQ(apply.commit(request), Status::Pending);
    const auto installed = input.poll();
    ASSERT_TRUE(installed.latest_poll()->has_context(original));
    EXPECT_FALSE(installed.latest_poll()->has_changes());
    EXPECT_EQ(apply.observe(request, input.idle()), Status::Pending); // Baseline is not evidence.
    EXPECT_EQ(operations.opens, 0u);
    EXPECT_FALSE(fs::exists(files.path()));
    EXPECT_EQ(apply.observe(request, installed), Status::Ready);
    EXPECT_EQ(apply.observe(request, input.idle()), Status::Ready); // Previously witnessed lease.
    ASSERT_EQ(apply.commit(request), Status::Applied);
    EXPECT_EQ(score.active_context(), original);
    EXPECT_EQ(file.load_or_defaults().value, apply.draft());
}

TEST(tutorial_settings_apply, consumed_polls) {
    ApplyCase test;
    test.apply.edit(changed);
    test.input.mapper.begin_poll();
    const auto request = test.apply.request(); // Attachment misses this already-open poll.
    const auto candidate = test.apply.pending_context();
    const auto old_poll = test.input.poll();
    EXPECT_FALSE(old_poll.latest_poll()->has_context(candidate));
    EXPECT_EQ(test.apply.observe(request, old_poll), Status::Pending);
    EXPECT_EQ(test.apply.commit(request), Status::Pending);
    EXPECT_THROW(static_cast<void>(test.apply.request()), std::logic_error);
    const auto published = test.input.poll();
    EXPECT_TRUE(published.latest_poll()->has_context(candidate));
    EXPECT_FALSE(published.latest_poll()->has_changes());
    EXPECT_EQ(test.apply.observe(request, test.input.idle()), Status::Pending);
    EXPECT_EQ(test.apply.commit(request), Status::Pending);
    EXPECT_EQ(test.operations.opens, 0u);
    test.preserved();
    EXPECT_EQ(test.apply.observe(request, published), Status::Ready);
    EXPECT_EQ(test.apply.observe(request, test.input.idle()), Status::Ready);
    ASSERT_EQ(test.apply.commit(request), Status::Applied);
    test.applied();
}

TEST(tutorial_settings_apply, edit_stales_request) {
    ApplyCase test;
    const auto stale = test.ready();
    const auto abandoned = test.apply.pending_context();
    test.apply.edit({350, true, Key::C});
    EXPECT_EQ(test.apply.commit(stale), Status::Stale);
    EXPECT_EQ(test.operations.opens, 0u);
    const auto request = test.apply.request();
    const auto candidate = test.apply.pending_context();
    EXPECT_NE(candidate, abandoned);
    const auto both = test.input.poll();
    EXPECT_EQ(test.apply.observe(stale, both), Status::Stale);
    EXPECT_EQ(test.apply.cancel(stale), Status::Stale);
    EXPECT_EQ(test.apply.status(request), Status::Pending);
    EXPECT_EQ(test.apply.pending_context(), candidate);
    EXPECT_EQ(test.apply.observe(request, both), Status::Ready);
    EXPECT_EQ(test.apply.commit(request), Status::Applied);
    EXPECT_EQ(test.active, (GameSettings{350, true, Key::C}));
    EXPECT_EQ(test.file.load_or_defaults().value, test.active);
    EXPECT_EQ(test.score.active_context(), candidate);
    EXPECT_EQ(test.operations.commits, 1u);
}

TEST(tutorial_settings_apply, cancel) {
    for (const bool published : {false, true}) {
        SCOPED_TRACE(published);
        ApplyCase test;
        test.apply.edit(changed);
        const auto request = test.apply.request();
        const auto abandoned = test.apply.pending_context();
        if (published)
            EXPECT_EQ(test.apply.observe(request, test.input.poll()), Status::Ready);
        EXPECT_EQ(test.apply.cancel(request), Status::Cancelled);
        EXPECT_EQ(test.apply.cancel(request), Status::Cancelled);
        EXPECT_EQ(test.apply.commit(request), Status::Cancelled);
        EXPECT_EQ(test.apply.observe(request, test.input.poll()), Status::Cancelled);
        test.preserved();
        EXPECT_EQ(test.operations.opens, 0u);
        EXPECT_EQ(test.apply.pending_context(), 0u);
        EXPECT_FALSE(test.input.poll().latest_poll()->has_context(abandoned));
        const auto retry = test.apply.request();
        EXPECT_NE(retry, request);
        EXPECT_EQ(test.apply.observe(retry, test.input.poll()), Status::Ready);
        EXPECT_EQ(test.apply.commit(retry), Status::Applied);
        test.applied();
    }
}

TEST(tutorial_settings_apply, invalid_values) {
    ApplyCase test;
    for (const auto draft : {GameSettings{0, false, Key::B}, GameSettings{200, false, Key::A}}) {
        test.apply.edit(draft);
        EXPECT_THROW(static_cast<void>(test.apply.request()), std::invalid_argument);
        EXPECT_EQ(test.apply.draft(), draft);
        EXPECT_EQ(test.apply.pending_context(), 0u);
        EXPECT_EQ(test.active, GameSettings{});
        EXPECT_EQ(test.score.active_context(), test.original);
        EXPECT_EQ(test.score.key(), Key::Space);
        EXPECT_EQ(Files::read(test.files.path()), predecessor);
        EXPECT_FALSE(fs::exists(test.files.pending()));
        EXPECT_EQ(test.operations.opens, 0u);
    }
    EXPECT_EQ(test.apply.commit(test.ready()), Status::Applied);
    test.applied();
}

TEST(tutorial_settings_apply, clear) {
    for (const bool ready : {false, true}) {
        ApplyCase test;
        test.apply.edit(changed);
        const auto request = test.apply.request();
        const auto candidate = test.apply.pending_context();
        const auto installed = test.input.poll();
        ASSERT_TRUE(installed.latest_poll()->has_context(candidate));
        if (ready)
            EXPECT_EQ(test.apply.observe(request, installed), Status::Ready);
        test.input.mapper.clear();
        EXPECT_TRUE(installed.latest_poll()->has_context(candidate));
        EXPECT_EQ(test.apply.status(request), Status::Invalidated);
        EXPECT_EQ(test.apply.commit(request), Status::Invalidated);
        EXPECT_EQ(test.apply.observe(request, installed), Status::Invalidated);
        test.preserved();
        EXPECT_EQ(test.apply.pending_context(), 0u);
        EXPECT_EQ(test.operations.opens, 0u);
        EXPECT_THROW(static_cast<void>(test.apply.request()), std::logic_error);
    }
}

TEST(tutorial_settings_apply, shutdown) {
    Files files;
    Files::write(files.path(), predecessor);
    Operations operations;
    SettingsFile file(files.path(), operations);
    auto mapper = std::make_unique<Actions::Bindings>();
    auto previous = mapper->action_snapshot();
    GameSettings active;
    ScoreInput score(mapper->action_contexts(), keyboard);
    const auto original = score.active_context();
    SettingsApply apply(active, score, file);
    apply.edit(changed);
    const auto request = apply.request();
    const auto candidate = apply.pending_context();
    const Tick installed(previous, {mapper->publish_actions()});
    EXPECT_EQ(apply.observe(request, installed), Status::Ready);
    mapper.reset();
    EXPECT_TRUE(installed.latest_poll()->has_context(original));
    EXPECT_TRUE(installed.latest_poll()->has_context(candidate));
    EXPECT_EQ(apply.observe(request, installed), Status::Invalidated);
    EXPECT_EQ(apply.commit(request), Status::Invalidated);
    EXPECT_EQ(active, GameSettings{});
    EXPECT_EQ(score.active_context(), original);
    EXPECT_EQ(score.key(), Key::Space);
    EXPECT_EQ(apply.draft(), changed);
    EXPECT_EQ(Files::read(files.path()), predecessor);
    EXPECT_EQ(operations.opens, 0u);
    EXPECT_THROW(static_cast<void>(apply.request()), std::logic_error); // Closed endpoint/invalid active lease.
}

TEST(tutorial_settings_apply, unchanged_invalidated) {
    ApplyCase test;
    test.apply.edit({200, false, Key::Space});
    const auto request = test.apply.request();
    EXPECT_EQ(test.apply.status(request), Status::Ready);
    test.input.mapper.clear();
    EXPECT_EQ(test.apply.commit(request), Status::Invalidated);
    EXPECT_EQ(test.operations.opens, 0u);
    EXPECT_EQ(Files::read(test.files.path()), predecessor);
    EXPECT_EQ(test.active, GameSettings{});
    EXPECT_EQ(test.score.active_context(), test.original);
}

TEST(tutorial_settings_apply, generation_invalidated) {
    ApplyCase test;
    const auto request = test.ready();
    test.score.set_generation(request + 1); // Exercise a violated lease/generation proof before save.
    EXPECT_EQ(test.apply.commit(request), Status::Stale);
    test.preserved();
    EXPECT_EQ(test.operations.opens, 0u);
    EXPECT_EQ(test.apply.pending_context(), 0u);
}

TEST(tutorial_settings_apply, no_autosave) {
    Files files;
    Operations operations;
    SettingsFile file(files.path(), operations);
    Polls input;
    GameSettings active;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    const auto original = score.active_context();
    Actions::ContextId dropped = 0;
    {
        SettingsApply apply(active, score, file);
        EXPECT_EQ(file.load_or_defaults().status, SettingsLoadStatus::Missing);
        apply.edit(changed);
        const auto request = apply.request();
        dropped = apply.pending_context();
        EXPECT_EQ(apply.observe(request, input.poll()), Status::Ready);
    }
    EXPECT_EQ(operations.opens, 0u);
    EXPECT_EQ(file.load_or_defaults().status, SettingsLoadStatus::Missing);
    EXPECT_EQ(active, GameSettings{});
    EXPECT_EQ(score.active_context(), original);
    EXPECT_EQ(score.key(), Key::Space);
    EXPECT_TRUE(input.poll().latest_poll()->has_context(dropped));
    EXPECT_FALSE(input.poll().latest_poll()->has_context(dropped));
    auto next = score.stage(Key::C);
    EXPECT_TRUE(next); // Destructor released its own candidate/slot.
}

TEST(tutorial_settings_apply, setup) {
    Files files;
    Files::write(files.path(), predecessor);
    Operations operations;
    SettingsFile file(files.path(), operations);
    Polls input;
    GameSettings active;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    auto invalid = GameSettings{0, true, Key::Space};
    EXPECT_THROW(static_cast<void>(SettingsApply(invalid, score, file)), std::invalid_argument);
    auto mismatch = changed;
    EXPECT_THROW(static_cast<void>(SettingsApply(mismatch, score, file)), std::invalid_argument);
    auto pending = score.stage(Key::C);
    ASSERT_TRUE(pending);
    const auto candidate = pending->id();
    EXPECT_THROW(static_cast<void>(SettingsApply(active, score, file)), std::logic_error);
    EXPECT_EQ(pending->id(), candidate);
    EXPECT_EQ(operations.opens, 0u);
    EXPECT_EQ(Files::read(files.path()), predecessor);
}

TEST(tutorial_settings_apply, exhausted_generation) {
    Files files;
    Files::write(files.path(), predecessor);
    Operations operations;
    SettingsFile file(files.path(), operations);
    Polls input;
    GameSettings active;
    ScoreInput score(input.mapper.action_contexts(), keyboard);
    const auto original = score.active_context();
    score.set_generation(std::numeric_limits<ScoreInput::Generation>::max());
    SettingsApply exhausted(active, score, file);
    EXPECT_THROW(exhausted.edit(changed), std::overflow_error);
    EXPECT_THROW(static_cast<void>(exhausted.request()), std::overflow_error);
    EXPECT_EQ(exhausted.draft(), GameSettings{});
    EXPECT_EQ(operations.opens, 0u);
    EXPECT_EQ(score.active_context(), original);
    EXPECT_EQ(Files::read(files.path()), predecessor);
}

TEST(tutorial_settings_apply, open_failure) {
    failure(Fault::Open, "open exclusive temporary");
}
TEST(tutorial_settings_apply, partial_write_failure) {
    failure(Fault::Write, "write");
}
TEST(tutorial_settings_apply, flush_failure) {
    failure(Fault::Flush, "flush");
}
TEST(tutorial_settings_apply, close_failure) {
    failure(Fault::Close, "close");
}
TEST(tutorial_settings_apply, rename_failure) {
    failure(Fault::Rename, "rename temporary");
}

TEST(tutorial_settings_apply, cleanup_failure) {
    ApplyCase test;
    const auto request = test.ready();
    test.operations.fault = Fault::Write;
    test.operations.cleanup_fails = true;
    try {
        static_cast<void>(test.apply.commit(request));
        FAIL() << "Expected partial write and owned cleanup failure";
    } catch (const SettingsFileError& error) {
        const std::string issue = error.what();
        EXPECT_NE(issue.find("write"), std::string::npos) << issue;
        EXPECT_NE(issue.find("injected Apply I/O"), std::string::npos) << issue;
        EXPECT_NE(issue.find("cleanup"), std::string::npos) << issue;
        EXPECT_NE(issue.find(test.files.pending().string()), std::string::npos) << issue;
    }
    test.preserved();
    EXPECT_EQ(test.apply.status(request), Status::Failed);
    EXPECT_EQ(test.apply.pending_context(), 0u);
    EXPECT_EQ(test.operations.cleanups, 1u);
    EXPECT_TRUE(test.operations.cleaned_owned);
    ASSERT_TRUE(fs::exists(test.files.pending()));
    const auto owned_partial_bytes = Files::read(test.files.pending());
    EXPECT_FALSE(owned_partial_bytes.empty());
    EXPECT_EQ(test.apply.commit(request), Status::Failed);
    EXPECT_EQ(Files::read(test.files.pending()), owned_partial_bytes);
    test.operations.fault = Fault::None;
    test.operations.cleanup_fails = false;
    ASSERT_TRUE(fs::remove(test.files.pending())); // Fixture removes only its own failed temporary.
    const auto retry = test.apply.request();
    EXPECT_EQ(test.apply.observe(retry, test.input.poll()), Status::Ready);
    EXPECT_EQ(test.apply.commit(retry), Status::Applied);
    test.applied();
}

TEST(tutorial_settings_apply, temporary_collision) {
    ApplyCase test;
    const auto request = test.ready();
    Files::write(test.files.pending(), "another attempt's bytes");
    EXPECT_THROW(static_cast<void>(test.apply.commit(request)), SettingsFileError);
    test.preserved();
    EXPECT_EQ(Files::read(test.files.pending()), "another attempt's bytes");
    EXPECT_EQ(test.operations.cleanups, 0u);
    EXPECT_EQ(test.operations.writes, 0u);
    EXPECT_EQ(test.apply.pending_context(), 0u);
    ASSERT_TRUE(fs::remove(test.files.pending())); // Pre-existing file belongs to this fixture.
    const auto retry = test.apply.request();
    EXPECT_EQ(test.apply.observe(retry, test.input.poll()), Status::Ready);
    EXPECT_EQ(test.apply.commit(retry), Status::Applied);
    test.applied();
}

TEST(tutorial_settings_apply, exceptional_failure) {
    for (const auto fault : {Fault::Allocation, Fault::Programming}) {
        ApplyCase test;
        const auto request = test.ready();
        test.operations.fault = fault;
        if (fault == Fault::Allocation)
            EXPECT_THROW(static_cast<void>(test.apply.commit(request)), std::bad_alloc);
        else
            EXPECT_THROW(static_cast<void>(test.apply.commit(request)), std::logic_error);
        test.preserved();
        EXPECT_EQ(test.apply.status(request), Status::Failed);
        EXPECT_EQ(test.apply.pending_context(), 0u);
        EXPECT_FALSE(fs::exists(test.files.pending()));
        EXPECT_EQ(test.operations.closes, 1u);
        EXPECT_EQ(test.operations.cleanups, 1u);
        test.operations.fault = Fault::None;
        const auto retry = test.apply.request();
        EXPECT_EQ(test.apply.observe(retry, test.input.poll()), Status::Ready);
        EXPECT_EQ(test.apply.commit(retry), Status::Applied);
        test.applied();
    }
}
