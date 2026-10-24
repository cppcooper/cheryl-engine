#pragma once
#include "channels.h"

namespace ProcGenChannelChecks {
    // <=512 cells. Independent per-source tracing/unit counts, degree/mask and
    // exact path-link coverage; no production rank accumulation/compression used.
    void inspect(ProcGen::ChannelInput input, const ProcGen::Channels& output);
}
