#pragma once

#include <string_view>

// Independent exact readings, authored before executing the renderer. Never generated from it.
namespace Literal {
    inline constexpr std::string_view S1 = R"(spoken release via contact after 0 ms {
  bind hand = willing touched actor at release ;
  together ordered {
    on hand bridge Common@1 { heal 12 hp instant } ;
  }
})";
    inline constexpr std::string_view S2 = R"(spoken release via burst after 1000 ms {
  bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ;
  together ordered {
    on near bridge Common@1 { fire damage 8 dp instant } ;
    on near bridge Common@1 { multiply speed by 750 permille for 2000 ms } ;
  }
})";
    inline constexpr std::string_view S3 = R"(spoken release via burst after 0 ms {
  bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ;
  together ordered {
    on near bridge Common@1 { multiply speed by 750 permille for 2000 ms } ;
  }
})";
    inline constexpr std::string_view S5 = R"(engraved item-touch charges 1 release via item-event after 0 ms {
  bind toucher = willing event toucher at activation ;
  together ordered {
    on toucher bridge Common@1 { heal 12 hp instant } ;
  }
})";
}
