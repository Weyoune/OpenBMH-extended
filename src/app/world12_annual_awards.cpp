// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_annual_awards.hpp"
#include <limits>
namespace bmh::app::world12 {
std::vector<bmh::world12::AnnualAward> publish_annual_awards(
    bmh::world12::CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,
    const bmh::GameState& reference,const bmh::TextDb& texts) {
    const auto qualified=[&]{return owner.scope&&owner.scope==expected.scope&&owner.revision==expected.revision&&owner.revision!=std::numeric_limits<uint64_t>::max();};
    if(!qualified())throw bmh::FormatError("world12: stale annual awards owner");
    const auto before=state;auto staged=bmh::world12::stage_annual_awards(state,reference,texts);
    auto candidate=state;candidate.world=std::move(staged.world);candidate.validate();
    if(state!=before||!qualified())throw bmh::FormatError("world12: annual awards owner changed before publication");
    state=std::move(candidate);++owner.revision;return std::move(staged.awards);
}
}
