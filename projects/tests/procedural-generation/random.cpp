#include "maze.h"

namespace ProcGen {
    namespace {
        constexpr std::uint64_t mix(std::uint64_t value) {
            value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
            value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
            return value ^ (value >> 31);
        }
    }

    std::uint64_t derive_stream_state(StreamKey key) {
        // Ordered word decoding of the documented canonical tuple; modulo 2^64.
        return mix(key.seed ^ mix(key.stage ^ UINT64_C(0x6a09e667f3bcc909)) ^ mix(key.stream ^ UINT64_C(0xbb67ae8584caa73b)));
    }

    IntegerStream IntegerStream::for_stage(StreamKey key) {
        return IntegerStream(derive_stream_state(key));
    }

    std::uint64_t IntegerStream::next() {
        state_ += UINT64_C(0x9e3779b97f4a7c15);
        return mix(state_);
    }

    bool Work::consume(Index units) {
        if (units > limit_ - used_) {
            return false;
        }
        used_ += units;
        return true;
    }

    Result<std::uint64_t> sample_below(IntegerStream& stream, std::uint64_t bound, Work& work) {
        if (bound == 0) {
            return std::unexpected(Failure{FailureKind::invalid_input, Reason::bound});
        }
        const auto threshold = (std::uint64_t{0} - bound) % bound;
        while (work.consume(1)) {
            const auto value = stream.next();
            if (value >= threshold) {
                return value % bound;
            }
        }
        return std::unexpected(Failure{FailureKind::budget_refused, Reason::work});
    }
}
