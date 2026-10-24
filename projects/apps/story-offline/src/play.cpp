#include "play.h"

#include <stdexcept>
#include <type_traits>

namespace OfflineStory {
    namespace {
        D::Selection issued(const D::View& v, std::string_view id) {
            for (const auto& row : v.choices)
                if (row.id.value == id)
                    return row.selection;
            throw std::logic_error("Missing setup choice");
        }
        Error observe(D::SessionError e) {
            return {Code::Resume, "play.offer", {}, 0, {}, e};
        }
        struct FrameLimit {};
        struct Frame {
            std::size_t cap;
            std::string text;
            void add(std::string_view s) {
                if (s.size() > cap - text.size())
                    throw FrameLimit{};
                text.append(s);
            }
            void number(int n) { add(std::to_string(n)); }
            void quantity(int n, std::string_view singular, std::string_view plural) {
                number(n);
                add(" ");
                add(n == 1 ? singular : plural);
            }
        };
        bool space(char c) {
            return c == ' ' || c == '\t' || c == '\r';
        }
        std::string_view trim(std::string_view s) {
            while (!s.empty() && space(s.front()))
                s.remove_prefix(1);
            while (!s.empty() && space(s.back()))
                s.remove_suffix(1);
            return s;
        }
        enum class Line { Ready, Eof, Limit, Invalid, Io };
        Line read_line(std::istream& in, std::array<char, 256>& bytes, std::size_t cap, std::size_t& size) {
            size = 0;
            try {
                while (true) {
                    const auto c = in.get();
                    if (c == std::char_traits<char>::eof())
                        return in.bad() || (!in.eof() && in.fail()) ? Line::Io : Line::Eof;
                    if (c == '\n')
                        return Line::Ready;
                    if (size == cap)
                        return Line::Limit; // At most cap+1 detection bytes, no draining.
                    if (static_cast<unsigned char>(c) < 32 && c != '\t' && c != '\r')
                        return Line::Invalid;
                    if (c == 127)
                        return Line::Invalid;
                    bytes[size++] = static_cast<char>(c);
                }
            } catch (const std::exception&) {
                return Line::Io;
            }
        }
        struct Output {
            std::ostream& out;
            std::ostream& err;
            std::size_t cap, used = 0;
            PlayExit failure = PlayExit::Complete;
            bool emit(std::string_view s, bool diagnostic = false) {
                if (s.size() > cap - used) {
                    failure = PlayExit::Limit;
                    return false;
                }
                used += s.size(); // Conservative charge even on a partial failed write.
                try {
                    auto& stream = diagnostic ? err : out;
                    stream.write(s.data(), static_cast<std::streamsize>(s.size()));
                    if (!stream) {
                        failure = PlayExit::Io;
                        return false;
                    }
                } catch (const std::exception&) {
                    failure = PlayExit::Io;
                    return false;
                }
                return true;
            }
            bool prompt() {
                if (!emit("> "))
                    return false;
                try {
                    out.flush();
                    if (!out) {
                        failure = PlayExit::Io;
                        return false;
                    }
                } catch (const std::exception&) {
                    failure = PlayExit::Io;
                    return false;
                }
                return true;
            }
            PlayExit finish(PlayExit status) {
                try {
                    out.flush();
                    if (!out)
                        return PlayExit::Io;
                    err.flush();
                    if (!err)
                        return PlayExit::Io;
                } catch (const std::exception&) {
                    return PlayExit::Io;
                }
                return status;
            }
            PlayExit stop(PlayExit reason, std::string_view message) {
                // Diagnostics share the same cap; an exhausted/broken stream is never retried.
                if (reason != PlayExit::Io && message.size() <= cap - used)
                    emit(message, true);
                return failure == PlayExit::Io ? PlayExit::Io : finish(reason);
            }
        };
        std::string_view rejection(const PlayError& e) {
            if (e.kind == PlayRefusal::Unknown)
                return "Unknown choice. Use a displayed choice ID.\n";
            if (e.kind == PlayRefusal::Phase)
                return "Choice refused: that offer belongs to another scene.\n";
            if (e.owner == D::SessionError::Unavailable)
                return "Choice refused: unavailable right now. No action was taken.\n";
            return "Choice refused: that offer is no longer current. Use show.\n";
        }
    }
    PlayDriver::PlayDriver(SelectedBargainHandle c, BargainEncounter b, D::View v, std::array<std::uint64_t, 2> effects)
    : selected_(std::move(c)), bargain_(std::move(b)), offer_(std::move(v)), preparation_effects_(effects) {}
    std::expected<PlayDriver, Error> PlayDriver::start(std::uint64_t seed) {
        const auto ferry_content = project(accept(manual_sets()[0], ferry_catalogue(), {}).value(), 0).value();
        auto ferry = Encounter::start(ferry_content, {"play-ferry"}).value();
        ferry.choose(issued(ferry.view().value(), "pay")).value();
        ferry.choose(issued(ferry.view().value(), "continue")).value();
        const auto watch_content = compose_watch(manual_library(), ferry_content, ferry.capture().value(), 0).value();
        auto watch = WatchEncounter::start(watch_content, {"play-watch"}).value();
        watch.choose(issued(watch.view().value(), "fare-record")).value();
        watch.choose(issued(watch.view().value(), "ask-again")).value();
        auto chosen = select_bargain(selection_library(), selection_catalogue(), {watch_content, watch.capture().value()}, seed, 1);
        if (!chosen)
            return std::unexpected(chosen.error().detail.value_or(Error{Code::Conflict, "play.selection", chosen.error().field}));
        auto bargain = BargainEncounter::start((*chosen)->content(), {"play-bargain"});
        if (!bargain)
            return std::unexpected(std::move(bargain.error()));
        auto offered = bargain->view();
        if (!offered)
            return std::unexpected(observe(offered.error()));
        return PlayDriver(std::move(*chosen), std::move(*bargain), std::move(*offered), {ferry.effects(), watch.effects()});
    }
    std::string_view PlayDriver::speaker() const noexcept {
        if (!selected_)
            return "Relief steward";
        for (const auto& actor : selected_->content()->catalogue().actors)
            if (actor.id == selected_->content()->request().actor)
                return actor.name;
        return "Relief steward"; // Only a programming defect could miss the validated actor.
    }
    std::expected<PlayChoice, PlayError> PlayDriver::resolve(std::string_view id) const {
        if (phase_ == PlayPhase::Complete)
            return std::unexpected(PlayError{PlayRefusal::Phase, {}});
        for (const auto& row : offer_.choices)
            if (row.id.value == id)
                return PlayChoice{phase_, row.selection};
        return std::unexpected(PlayError{PlayRefusal::Unknown, {}});
    }
    std::expected<void, PlayError> PlayDriver::apply(const PlayChoice& choice) {
        if (phase_ == PlayPhase::Complete || choice.phase != phase_)
            return std::unexpected(PlayError{PlayRefusal::Phase, {}});
        auto result = phase_ == PlayPhase::Bargain ? bargain_.choose(choice.selection) : payoff_->choose(choice.selection);
        if (!result)
            return std::unexpected(PlayError{PlayRefusal::Owner, result.error()});
        auto displayed = refresh();
        if (!displayed)
            throw std::runtime_error("Committed choice could not be observed");
        return {};
    }
    std::expected<void, D::SessionError> PlayDriver::refresh() {
        auto next = phase_ == PlayPhase::Bargain ? bargain_.view() : payoff_->view();
        if (!next)
            return std::unexpected(next.error());
        offer_ = std::move(*next);
        if (phase_ == PlayPhase::Payoff && offer_.kind == D::NodeKind::Terminal)
            phase_ = PlayPhase::Complete;
        return {};
    }
    std::expected<void, Error> PlayDriver::advance() {
        if (phase_ != PlayPhase::Bargain || offer_.kind != D::NodeKind::Terminal)
            return std::unexpected(Error{Code::Context, "play.phase", {}});
        const auto completed = bargain_.capture();
        if (!completed)
            return std::unexpected(observe(completed.error()));
        const bool pending =
            completed->state.resolution == ReliefResolution::Promised || completed->state.resolution == ReliefResolution::Negotiated;
        auto content = compose_payoff(
            selected_->content(), *completed, {selected_->content()->request().location, selected_->content()->request().actor, pending},
            manual_payoff_library()
        );
        if (!content)
            return std::unexpected(std::move(content.error()));
        auto owner = PayoffEncounter::start(*content, {"play-payoff"});
        if (!owner)
            return std::unexpected(std::move(owner.error()));
        auto displayed = owner->view();
        if (!displayed)
            return std::unexpected(observe(displayed.error()));
        static_assert(std::is_nothrow_move_constructible_v<PayoffEncounter> && std::is_nothrow_move_assignable_v<D::View>);
        std::expected<void, Error> success;
        payoff_.emplace(std::move(*owner));
        payoff_content_ = std::move(*content);
        offer_ = std::move(*displayed);
        phase_ = PlayPhase::Payoff;
        return success;
    }
    std::expected<void, Error> PlayDriver::presence(bool value) {
        if (phase_ != PlayPhase::Payoff)
            return std::unexpected(Error{Code::Context, "play.presence", {}});
        auto moved = payoff_->handoff(value);
        if (!moved)
            return moved;
        auto displayed = refresh();
        if (!displayed)
            return std::unexpected(observe(displayed.error()));
        return {};
    }
    std::string PlayDriver::render(std::size_t cap) const {
        if (cap > maximum_play_limits.frame_bytes)
            throw FrameLimit{};
        if (!selected_)
            throw std::logic_error("Moved-from play driver");
        Frame f{cap};
        f.add(phase_ == PlayPhase::Bargain ? "\nRelief bargain - " : "\nRelief handoff - ");
        f.add(speaker());
        f.add("\n");
        f.add(offer_.text);
        f.add("\nInventory: ");
        const auto& world = payoff_ ? payoff_->state().world : bargain_.state().world;
        f.quantity(world.facts.coins, "coin", "coins");
        f.add(", ");
        f.quantity(world.facts.planks, "plank", "planks");
        f.add(".\nCommitment: ");
        if (payoff_) {
            const auto& s = payoff_->state();
            if (s.obligation == Obligation::Pending) {
                f.quantity(s.due, "plank", "planks");
                f.add(" pending");
            } else if (s.obligation == Obligation::Fulfilled) {
                f.add("fulfilled; ");
                f.quantity(s.delivered, "plank delivered", "planks delivered");
            } else if (s.obligation == Obligation::Broken)
                f.add("broken; no delivery");
            else
                f.add("none");
            f.add(". Favor: ");
            f.add(s.favor ? "one relief favor" : "none");
            f.add(".\n");
            if (phase_ != PlayPhase::Complete)
                f.add(s.at_handoff ? "You are at the handoff.\n" : "You are away from the handoff. Use approach to meet the steward.\n");
        } else {
            const auto& s = bargain_.state();
            if (s.planks_due) {
                f.quantity(s.planks_due, "plank", "planks");
                f.add(" pending; no delivery reward");
            } else
                f.add("none");
            f.add(".\n");
            if (s.terms_known) {
                f.add("Known revised promise: ");
                f.quantity(selected_->content()->rule().negotiated_planks, "plank later", "planks later");
                f.add(".\n");
            }
        }
        for (const auto& row : offer_.choices) {
            f.add("  ");
            f.add(row.id.value);
            f.add(" - ");
            f.add(row.text);
            f.add(row.available ? " [available]" : " [unavailable]");
            if (!payoff_) {
                const auto& rule = selected_->content()->rule();
                for (const auto& action : selected_->content()->sets().actions)
                    if (action.id == row.id.value) {
                        if (action.kind == ReliefIntent::Help) {
                            f.add("; cost: ");
                            f.quantity(rule.now_planks, "plank now", "planks now");
                        }
                        if (action.kind == ReliefIntent::Pay) {
                            f.add("; cost: ");
                            f.quantity(rule.coins, "coin now", "coins now");
                        }
                        if (action.kind == ReliefIntent::Promise || action.kind == ReliefIntent::Commit) {
                            f.add("; promise: ");
                            f.quantity(
                                action.kind == ReliefIntent::Promise ? rule.promise_planks : rule.negotiated_planks, "plank later",
                                "planks later"
                            );
                            f.add("; no reward now");
                        }
                    }
            } else if (row.id.value == "deliver") {
                f.add("; cost: ");
                f.quantity(payoff_->state().due, "plank", "planks");
                f.add("; one relief favor on delivery");
            }
            f.add("\n");
        }
        return std::move(f.text);
    }
    std::string_view play_help() noexcept {
        return "Commands: choose ID, show, help, approach, step-away, quit.\nChoose only an offered ID. Close the bargain explicitly, then approach the handoff.\n";
    }
    PlayExit run_play(PlayDriver& driver, std::istream& input, std::ostream& out, std::ostream& err, const PlayLimits& limits) {
        Output io{out, err, limits.output_bytes};
        for (auto field : {&PlayLimits::commands, &PlayLimits::line_bytes, &PlayLimits::output_bytes, &PlayLimits::frame_bytes})
            if (limits.*field > maximum_play_limits.*field)
                return io.stop(PlayExit::Limit, "Play stopped: invalid limits.\n");
        const auto display = [&]() { return io.emit(driver.render(limits.frame_bytes)); };
        try {
            if (!io.emit("You paid 3 coins to cross by ferry and showed your fare record at the quay.\n"))
                return io.failure;
            if (!io.emit(play_help()) || !display())
                return io.failure;
            for (std::size_t count = 0;; ++count) {
                if (driver.phase() == PlayPhase::Complete) {
                    if (!io.emit("Encounter finished.\n"))
                        return io.failure;
                    return io.finish(PlayExit::Complete);
                }
                if (count == limits.commands)
                    return io.stop(PlayExit::Limit, "Play stopped: command limit.\n");
                if (!io.prompt())
                    return io.failure;
                std::array<char, 256> line;
                std::size_t size = 0;
                const auto read = read_line(input, line, limits.line_bytes, size);
                if (read == Line::Io)
                    return PlayExit::Io;
                if (read == Line::Limit || read == Line::Invalid)
                    return io.stop(PlayExit::Limit, "Play stopped: oversized or malformed input line.\n");
                if (read == Line::Eof) {
                    if (!io.emit("Play ended early; no choice is selected automatically.\n"))
                        return io.failure;
                    return io.finish(PlayExit::Early);
                }
                const auto command = trim(std::string_view(line.data(), size));
                if (command == "quit") {
                    if (!io.emit("Play ended early; no choice is selected automatically.\n"))
                        return io.failure;
                    return io.finish(PlayExit::Early);
                }
                if (command == "help") {
                    if (!io.emit(play_help()))
                        return io.failure;
                    continue;
                }
                if (command == "show") {
                    if (!driver.refresh())
                        return io.stop(PlayExit::Failure, "Play stopped: cannot inspect this owner.\n");
                    if (!display())
                        return io.failure;
                    continue;
                }
                if (command == "approach" || command == "step-away") {
                    if (!driver.presence(command == "approach")) {
                        if (!io.emit("Presence refused: close the bargain first.\n"))
                            return io.failure;
                        continue;
                    }
                    if (!display())
                        return io.failure;
                    continue;
                }
                if (!command.starts_with("choose ") && !command.starts_with("choose\t")) {
                    if (!io.emit("Unknown command. Use help or choose ID.\n"))
                        return io.failure;
                    continue;
                }
                auto id = trim(command.substr(6));
                if (id.empty() || id.find_first_of(" \t\r") != std::string_view::npos) {
                    if (!io.emit("Usage: choose ID.\n"))
                        return io.failure;
                    continue;
                }
                auto resolved = driver.resolve(id);
                if (!resolved) {
                    if (!io.emit(rejection(resolved.error())))
                        return io.failure;
                    continue;
                }
                auto applied = driver.apply(*resolved);
                if (!applied) {
                    if (applied.error().owner == D::SessionError::Faulted || applied.error().owner == D::SessionError::Busy ||
                        applied.error().owner == D::SessionError::MovedFrom)
                        return io.stop(PlayExit::Failure, "Play stopped: owner unavailable.\n");
                    if (!io.emit(rejection(applied.error())))
                        return io.failure;
                    continue;
                }
                if (!display())
                    return io.failure;
                if (driver.phase() == PlayPhase::Bargain && driver.offer().kind == D::NodeKind::Terminal) {
                    if (!driver.advance())
                        return io.stop(PlayExit::Failure, "Play stopped: follow-up preparation failed.\n");
                    if (!display())
                        return io.failure;
                }
            }
        } catch (const FrameLimit&) {
            return io.stop(PlayExit::Limit, "Play stopped: display limit.\n");
        } catch (const std::exception&) {
            return io.stop(PlayExit::Failure, "Play stopped: preparation or observation failed; prior actions may have committed.\n");
        }
    }
    int play(std::uint64_t seed, std::istream& in, std::ostream& out, std::ostream& err) {
        try {
            auto driver = PlayDriver::start(seed);
            if (!driver)
                return static_cast<int>(PlayExit::Failure);
            return static_cast<int>(run_play(*driver, in, out, err));
        } catch (const std::exception&) {
            return static_cast<int>(PlayExit::Failure);
        }
    }
}
