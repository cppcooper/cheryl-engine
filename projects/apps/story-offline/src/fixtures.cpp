#include "story.h"

namespace OfflineStory {
    Catalogue ferry_catalogue() {
        return {
            {{"mara", "Mara the ferrymaster"}, {"ivo", "Ivo of the harbor watch"}, {"lena", "Lena inland"}},
            {"pay_fare", "repair_hull", "decline"},
            {"can_pay", "can_repair", "can_decline"},
            {"passage"},
            {{"fare-three", "mara", "pay_fare", "can_pay", Method::Pay, {3, 0, true, true, false, false, {1, -1, 0}}, {"passage"}},
             {"patch-two", "mara", "repair_hull", "can_repair", Method::Repair, {0, 2, true, false, true, false, {3, -2, 0}}, {"passage"}},
             {"fare-five", "mara", "pay_fare", "can_pay", Method::Pay, {5, 0, true, true, false, false, {1, -1, 0}}, {"passage"}},
             {"brace-one", "mara", "repair_hull", "can_repair", Method::Repair, {0, 1, true, false, true, false, {4, -3, 0}}, {"passage"}},
             {"remain", "mara", "decline", "can_decline", Method::Decline, {0, 0, false, false, false, true, {0, 0, 0}}, {}}}
        };
    }
    std::vector<CandidateSet> manual_sets() {
        const auto catalogue = ferry_catalogue();
        const auto action = [&](std::string id, std::size_t outcome, std::vector<Flavour> flavours) {
            const auto& rule = catalogue.outcomes[outcome];
            return Action{std::move(id), rule.actor,       rule.operation, rule.condition,
                          rule.id,       rule.consequence, rule.rewards,   std::move(flavours)};
        };
        return {
            {Origin::Manual,
             "tide",
             "The last ferry waits. Mara needs fare or two sound planks.",
             1,
             {action(
                  "pay", 0,
                  {{"plain", "Pay three coins", "Mara pockets the fare. Your seat is ready."},
                   {"river", "Let three coins speak for me", "Three bright circles buy a quiet crossing."}}
              ),
              action(
                  "repair", 1,
                  {{"plain", "Offer two planks to patch the hull", "Mara fits your planks. This crossing is on her."},
                   {"river", "Give the river a sounder boat", "New boards answer the river. Mara remembers your hands."}}
              ),
              action(
                  "decline", 4,
                  {{"plain", "Stay on this bank", "Mara casts off. You keep your supplies."},
                   {"river", "Keep my coins and the shore", "The rope slips free. The bank remains yours."}}
              )}},
            {Origin::Manual,
             "storm",
             "Floodwater is rising. A seat costs five coins, or one plank can brace the ramp.",
             2,
             {action(
                  "pay", 2,
                  {{"plain", "Pay five coins for the flood crossing", "Mara takes the storm fare and holds a seat."},
                   {"river", "Buy a dry seat with five coins", "The river roars; five coins keep your place aboard."}}
              ),
              action(
                  "repair", 3,
                  {{"plain", "Brace the ramp with one plank", "Mara steadies the ramp. You have earned passage."},
                   {"river", "Lend one board against the flood", "Your board holds. Mara's gratitude outlasts the crossing."}}
              ),
              action(
                  "decline", 4,
                  {{"plain", "Wait out the flood ashore", "Mara nods. This risk is not yours today."},
                   {"river", "Let the flood pass without me", "You choose the higher bank and an unspent purse."}}
              )}}
        };
    }
}
