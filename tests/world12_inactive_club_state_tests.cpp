// SPDX-License-Identifier: GPL-3.0-only
#include "inactive_club_state_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/inactive_club_state.hpp"
#include "game/world12/codec.hpp"
#include "cli/json.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
World* disturbed=nullptr;void disturb(int){draw_hook=nullptr;++disturbed->trend_rotation;}
}
int main()try {
    auto tables=bmh::test::authored_fixture_tables();tables[4]={"abcd","acdb","adbc"};set_fixture_tables(tables);
    const auto master=bmh::test::world12::inactive_master_fixture();const auto reference=bmh::test::world12::inactive_reference_fixture();const auto de=bmh::test::world12::inactive_rules_fixture(false),en=bmh::test::world12::inactive_rules_fixture(true);
    for(auto primary:{Country::germany,Country::england}){
        GameRandom game(17);WatcomRandom random(91);auto world=bmh::test::world12::inactive_national_fixture(master,reference,primary,game,random);const auto source=world;const auto gs=game,ws_game=game;const auto ws=random;
        const auto initialized=initialize_inactive_club_state(world,master,de,en,reference,game,random);
        check(initialized.initial_national_field.size()==4&&initialized.game_draws==(primary==Country::germany?180u:228u),"inactive initial Source field/cup shuffle count changed");
        auto unchanged=world;unchanged.inactive_club_state.reset();check(unchanged==source&&world.clubs.size()==62&&!world.transfer_market&&!world.economy&&!world.initial_competitions&&!world.european_campaigns,"inactive setup manufactured club gameplay/finance or altered actual population");
        GameRandom repeat_game=gs;WatcomRandom repeat_random=ws;auto repeat=source;const auto repeated=initialize_inactive_club_state(repeat,master,de,en,reference,repeat_game,repeat_random);check(repeat==world&&repeat_game.seed()==game.seed()&&repeat_random.seed()==random.seed()&&repeated.initial_national_field==initialized.initial_national_field,"inactive initial draw was nondeterministic");
        const auto before_draws=std::pair{game.calls(),random.calls()};prepare_national_campaign_from_initial_field(world,de,primary,initialized.initial_national_field,game,random);check(before_draws==std::pair{game.calls(),random.calls()},"actual national owner duplicated the Source initial draw");
        for(uint32_t season=0;season<8;++season){
            while(!world.national_campaigns->seasons.at(season).complete){const auto& campaign=world.national_campaigns->seasons.at(season);NationalResults scores{season,campaign.revision,{}};for(const auto& group:prepare_national_groups(campaign))scores.groups.push_back({group.source_entry,group.played,std::vector<uint16_t>(group.matches.size(),uint16_t((group.flags&8)?((1<<5)|2):(2<<5)))});commit_national_results(world,scores,de,random);}
            check(decode_world(encode_world(world))==world,"inactive state/source pool identities failed codec continuation");
            const auto before=world;const auto old_g=game;const auto old_w=random;rejects([&]{advance_inactive_club_state(world,season,de,en,game,random);},"inactive annual draw replay accepted");check(world==before&&game.seed()==old_g.seed()&&random.seed()==old_w.seed(),"inactive replay leaked Source random state");
            prepare_national_campaign(world,de,season+1,game,random,primary);const auto actual_national=world.national_campaigns;const auto next=advance_inactive_club_state(world,season+1,de,en,game,random);check(next.first==(primary==Country::germany?176u:224u)&&world.inactive_club_state->season==season+1&&world.national_campaigns==actual_national,"inactive yearly field reset changed actual national results/draw or Source shuffle count");
        }
        auto changed=source;auto changed_game=gs;auto changed_random=ws;disturbed=&changed;draw_hook=disturb;rejects([&]{initialize_inactive_club_state(changed,master,de,en,reference,changed_game,changed_random);},"inactive draw overwrote a changed external owner");draw_hook=nullptr;auto externally_changed=source;++externally_changed.trend_rotation;check(changed==externally_changed&&changed_game.seed()==gs.seed()&&changed_random.seed()==ws.seed(),"inactive cancellation leaked candidate or rewound external change");
        const auto bytes=encode_world(world);auto json=cli::parse_json(std::string(bytes.begin(),bytes.end()));json.object()->at("version")=41;const auto old=cli::dump_json(json);rejects([&]{decode_world(Bytes(old.begin(),old.end()));},"forged41 dropped actual inactive Source fields");(void)ws_game;
    }
    std::cout<<"Inactive Source setup: real full-master identities, zero duplicate national draw, DE/EN Source shuffles, eight actual controlled-score national programmes with staged inactive annual fields, codec/stale/replay passed (no club games or played calendar seasons)\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
