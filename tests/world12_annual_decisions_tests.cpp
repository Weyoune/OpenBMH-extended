// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "runtime_fixture_tables.hpp"
#include "formats/fbm.hpp"
#include "formats/savestate.hpp"
#include "game/schedule.hpp"
#include "game/world12/season_transition.hpp"
#include "game/world12/manager_creation.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/codec.hpp"
#include "game/season.hpp"
#include "game/messages.hpp"
#include "cli/json.hpp"
#include <iostream>
#include <bit>
#include <numeric>
#include <stdexcept>

namespace {
using namespace bmh;
using namespace bmh::world12;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,why);}
World master() {
    auto w=select_world_scope(bmh::test::world12::complete_world(),{true,true,false});
    for(auto country:{Country::germany,Country::england})for(int i=0;i<8;++i) {
        ClubRecord c;c.id={uint32_t(900000+int(country)*100+i)};c.country=country;c.name="Amateur "+std::to_string(i);c.crest={c.id.value};
        w.crests.emplace(c.crest,CrestResource{"C/TEST.VGA",0});w.clubs.emplace(c.id,c);
    }
    w.validate();return w;
}
TextDb texts() {
    TextDb t;for(size_t i=0;i<TEXT_COUNT;++i){const auto line="Synthetic notice "+std::to_string(i);t.strings.push_back(line);t.offsets.push_back(t.blob.size());t.blob.insert(t.blob.end(),line.begin(),line.end());t.blob.push_back(0);}return t;
}
CareerSnapshot state_fixture() {
    CareerSnapshot s;s.world=master();const auto de=initial_annual_amateur_pool(s.world,Country::germany),en=initial_annual_amateur_pool(s.world,Country::england);
    s.world.amateur_slots=std::array<std::vector<ClubId>,2>{de.slots,en.slots};
    s.world.players_initialized=true;s.world.markets.emplace();s.world.free_players.emplace();s.world.economy.emplace();s.world.settings=CareerSettings{3,2,5,4,false};
    s.transfers.emplace();s.results=SeasonResults::empty(s.world.catalogue);s.date=365;
    for(const auto& l:s.world.catalogue.leagues){s.league_rounds[l.id.value]=uint8_t(l.rounds());for(int r=1;r<=l.rounds();++r)for(int m=0;m<l.clubs/2;++m)s.results.result(s.world.catalogue,l.id,r,m)={true,uint8_t((r+m)%4),uint8_t((r+2*m)%3)};}
    s.world.initial_competitions.emplace();auto& fields=*s.world.initial_competitions;fields.manager_services_complete=true;
    auto club=s.world.clubs.begin();const size_t sizes[]={64,24,32};
    for(size_t k=0;k<3;++k){for(size_t i=0;i<sizes[k];++i,++club)fields.fields[k].push_back(club->first);fields.holders[k]=fields.fields[k].front();}
    s.world.transfer_market.emplace();s.validate();return s;
}
Manager manager(ClubId club,LeagueId league,uint8_t seat=0) {
    Manager m{seat,"Manager "+std::to_string(seat),club,league,seat};m.finance=ManagerFinance{};m.tactics=ManagerTactics{};m.match_state=ManagerMatchState{};m.career=ManagerCareerState{};
    m.career->season_records.emplace();m.career->season_services.emplace();m.career->season_services->promotion_tail_flags=0x35;return m;
}
AnnualDecisionContext context(const CareerSnapshot& s,Country c) {
    return {{c,(*s.world.amateur_slots)[c==Country::germany?0:1]},c==Country::germany?RegionalPromotion::champions_only:RegionalPromotion::not_applicable,{}};
}
GameState club_swap_fixture() {
    GameState g;g.club_ids.resize(168);g.club_heap_bc.resize(336);g.club_heap_78.resize(336);g.club_heap_b8.resize(336);g.team_morale.resize(168);g.team_flags.resize(168);g.position_history.resize(138*26);g.squad_slots.resize(3072,0xfff);
    std::iota(g.club_ids.begin(),g.club_ids.end(),uint16_t(0));
    g.memory.add_block(layout::ClubShortNameOffsets,168);g.memory.add_block(layout::ClubKitSelectors,168);
    g.memory.add_block(layout::Titles,900);g.memory.add_block(layout::Career_CompetitionPointersEnglish_ExecutableField28,4);
    g.memory.add_block(layout::CareerSession_FormatError_RuntimeField6,1);g.memory.set8(layout::CareerSession_FormatError_RuntimeField6,1);
    g.memory.add_block(layout::Career_RemoveJoinLoans_RuntimeField,800);
    g.memory.add_block(layout::DeskRecords_Teams_RuntimeField,276);
    return g;
}
void kernel_oracles() {
    // Independent old Source loops compare rejection sampling and draw counts.
    for(int n:{18,20})for(int league:{0,1,2})for(uint32_t seed:{1u,29u,589388519u}) {
        std::vector<uint8_t> ranks(size_t(n),0);for(int i=0;i<n;++i)ranks[size_t(i)]=uint8_t((i*7)%n);
        // Supply a permutation even for league sizes where7 is not coprime.
        std::iota(ranks.begin(),ranks.end(),uint8_t(0));std::rotate(ranks.begin(),ranks.begin()+3,ranks.end());
        auto human=[](size_t p){return p==2||p==5;};GameRandom oracle_game(0),game(0);WatcomRandom oracle_random(seed),random(seed);std::optional<size_t> expected;
        if(league<2&&oracle_game.range(0,12-7*league)==0){size_t r;do{do r=size_t(oracle_random.range(0,n-1));while(human(r));}while(ranks[r]<10||ranks[r]>15);expected=r;}
        auto actual=select_ai_licence_withdrawal(league,2,ranks,human,game,random);
        check(actual==expected&&game.seed()==oracle_game.seed()&&game.calls()==oracle_game.calls()&&random.seed()==oracle_random.seed()&&random.calls()==oracle_random.calls(),"AI licence Source rejection sequence changed");
    }
    for(uint32_t seed:{1u,29u,589388519u}) {
        WatcomRandom oracle(seed),random(seed);std::vector<int> pool{138,139,140,141,142,143,144,145};std::vector<int> expected;
        // Interior nameless slots reject; the nameless tail is trimmed.
        auto named=[](int c){return c!=140&&c<144;};int count=8;while(count>1&&!named(138+count-1))--count;
        for(int i=0;i<3;++i){int t;do t=oracle.range(138,138+count-1);while(std::find(expected.begin(),expected.end(),t)!=expected.end()||!named(t));expected.push_back(t);}
        const auto actual=draw_season_newcomers(3,pool,named,random);
        check(actual==expected&&random.seed()==oracle.seed()&&random.calls()==oracle.calls(),"newcomer Source rejection sequence changed");
    }
    check(!season_licence_broke({0,0,-6000000,0,0})&&season_licence_broke({0,0,-6000001,0,0}),"licence asset threshold changed");
    check(!season_licence_broke({0,65,0,0,99999})&&season_licence_broke({0,65,0,0,100000}),"transfer-ban threshold changed");
}
void tail_capture() {
    GameState g;g.manager_records.assign(0xb0c,0);g.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);g.memory.add_block(layout::Spending,4*0x24);
    for(size_t p=0;p<10;++p)g.manager_records[0x8b6+p]=255;
    g.manager_records[0xb05]=0x33;g.manager_records[0xb06]=0x35;g.manager_records[0xb07]=0x37;
    auto career=manager_career_state_from_legacy(g,0,{},true);check(career.season_services->promotion_tail_flags==0x35,"fresh Source promotion tail not captured");
    auto unknown=career;unknown.season_services->promotion_tail_flags.reset();
    check(!manager_career_state_from_legacy(g,0,{},true,&unknown).season_services->promotion_tail_flags,"old unknown promotion tail invented");
    mark_promoted_manager(g,0);check(g.manager_records[0xb06]==0xb5&&g.manager_records[0xb05]==0x33&&g.manager_records[0xb07]==0x37,"Source eighteenth flag touched neighbours");
    *career.season_services->promotion_tail_flags|=0x80;install_manager_career_state(g,0,career,{});
    check(g.manager_records[0xb06]==0xb5&&g.manager_records[0xb05]==0x33&&g.manager_records[0xb07]==0x37,"typed tail install touched neighbouring bytes");
}
void original_tail_fixture(const std::filesystem::path& data,const std::filesystem::path& save) {
    const auto exe=bmh::test::load_executable(data/"BMH.exe");const auto file=load_savegame(save);
    check(file.checksum_ok&&file.version=="V2.24","Original tail fixture version/checksum");
    const DbCounts counts{exe.u32(layout::CareerSession_DataDirectory_RuntimeField),exe.u32(layout::CareerSession_DataDirectory_RuntimeField3),exe.u32(layout::CareerSession_DataDirectory_RuntimeField2),exe.u32(layout::CareerSession_DataDirectory_RuntimeField4)};
    const auto original=GameState::from_save(SaveState(file.stream,counts),counts);
    std::vector<PlayerId> players;for(size_t i=0;i<original.players.size();++i)players.push_back({uint32_t(100000+i)});
    for(size_t manager=0;manager<original.manager_teams.size()&&manager<4;++manager) {
        auto source=original,typed=original;
        auto career=manager_career_state_from_legacy(typed,uint8_t(manager),players,true);
        const auto at=manager*0xb0c;
        CareerSnapshot canonical;canonical.world.markets.emplace();canonical.world.economy.emplace();canonical.transfers.emplace();
        std::copy_n(original.stocks.begin(),canonical.world.markets->stocks.size(),canonical.world.markets->stocks.begin());
        for(size_t b=0;b<canonical.world.markets->funds.size();++b)canonical.world.markets->funds[b]=original.memory.u8(layout::FundBlock+uint32_t(b));
        auto& economy=canonical.world.economy->managers[manager];
        const auto word=[&](size_t offset){uint32_t value=0;for(size_t b=0;b<4;++b)value|=uint32_t(original.manager_records[at+offset+b])<<(b*8);return value;};
        economy.deposit=word(0x318);
        for(size_t lender=0;lender<5;++lender)for(size_t field=0;field<6;++field)economy.loans[lender][field]=original.memory.u32(layout::Loans+uint32_t(manager*0x78+lender*0x18+field*4));
        Manager owned;owned.seat=uint8_t(manager);owned.finance=manager_finance_from_legacy(original,int(manager));owned.career=career;owned.match_state.emplace();owned.match_state->point_deduction=original.manager_records[at+0x220];canonical.world.managers.push_back(owned);
        canonical.transfers->balance[manager]=std::bit_cast<int32_t>(word(0x330));
        check(annual_licence_account(canonical,uint8_t(manager))==bmh::season_licence_account(original,int(manager)),"actual Original licence financial operands differ");
        check(career.season_services->promotion_tail_flags==original.manager_records[at+0xb06],"actual Original promotion tail was not captured");
        mark_promoted_manager(source,int(manager));
        for(auto& row:career.season_services->advertising_contracts)row[6]|=0x80;
        *career.season_services->promotion_tail_flags|=0x80;
        install_manager_career_state(typed,uint8_t(manager),career,players);
        for(size_t byte=0xb00;byte<0xb0c;++byte)
            check(typed.manager_records[at+byte]==source.manager_records[at+byte],"actual Original tail neighbour/taker bytes differ");
    }
    std::cout<<"Actual supplied Original licence operands and all twelve promotion tail bytes match Source\n";
}
void schema36() {
    auto state=state_fixture();state.world.managers.push_back(manager(state.world.membership[0].clubs[0],{0}));
    const auto encoded=encode_world(state.world);check(decode_world(encoded)==state.world,"schema36 lost canonical pool/tail state");
    auto root=cli::parse_json(std::string(encoded.begin(),encoded.end()));root.object()->at("version")=cli::Json(35);
    auto raw=cli::dump_json(root);rejects([&]{decode_world(Bytes(raw.begin(),raw.end()));},"schema35 silently accepted known36 state");
    root.object()->erase("amateur_slots");
    auto& managers=std::get<cli::Json::Array>(root.object()->at("managers").value);
    auto& services=*managers[0].object()->at("career").object()->at("season_services").object();
    services.erase("promotion_tail_flags");raw=cli::dump_json(root);const auto old=decode_world(Bytes(raw.begin(),raw.end()));
    check(!old.amateur_slots&&!old.managers[0].career->season_services->promotion_tail_flags,"old schema invented unknown pool or promotion tail");
    check(decode_world(encode_world(old))==old,"schema36 did not preserve old unknown state");
    auto duplicate=state.world;(*duplicate.amateur_slots)[0][1]=(*duplicate.amateur_slots)[0][0];rejects([&]{encode_world(duplicate);},"duplicate canonical amateur slot accepted");
    auto foreign=state.world;(*foreign.amateur_slots)[0][0]=(*foreign.amateur_slots)[1][0];rejects([&]{encode_world(foreign);},"foreign canonical amateur slot accepted");
    auto absent=state.world;absent.amateur_slots.reset();rejects([&]{initial_annual_amateur_pool(absent,Country::germany);},"active career inferred missing amateur order");
}
void annual_models() {
    auto s=state_fixture();const auto text=texts();auto c=context(s,Country::germany);
    const auto before=s;const auto initial=evaluate_annual_promotion_decisions(s,c);
    check(initial.game_draws==2&&initial.watcom_draws>3&&initial.licence_notices.size()==2,"real AI licences or rejection draws were skipped");
    auto withdrawn=initial.plan.standings[0][8].club;auto m=manager(withdrawn,{0});m.finance->balance=-7000000;s.world.managers.push_back(m);
    auto& economy=s.world.economy->managers[0];economy.deposit=500;economy.loans[0][1]=1000;economy.loans[0][5]=100;
    auto& market=*s.world.markets;market.stocks[0x50]=5;market.stocks[0x4e]=20;market.funds[0]=44;market.funds[1]=1;market.funds[20]=2;
    const auto account=annual_licence_account(s,0);check(account.assets==-6998800&&account.remaining_loans==900&&account.creditworthiness==0,"canonical account/deposit/funds/shares/loan operands changed");
    auto shifted=s;shifted.world.managers[0].seat=3;shifted.world.managers[0].portrait=3;
    shifted.world.economy->managers[3]=shifted.world.economy->managers[0];shifted.world.economy->managers[0]={};
    shifted.world.markets->stocks[0x56]=5;shifted.world.markets->stocks[0x50]=0;shifted.world.markets->funds[26]=2;shifted.world.markets->funds[20]=0;
    shifted.transfers->balance[3]=shifted.transfers->balance[0];shifted.transfers->balance[0]=0;
    check(annual_licence_account(shifted,3)==account,"canonical seat3 portfolio/loan operands used another manager");
    const auto decision=evaluate_annual_promotion_decisions(s,c);check(decision.licence_notices.front().club==withdrawn&&decision.licence_notices.front().manager==0,"bankrupt human Source licence was not first");
    check(std::count_if(decision.licence_notices.begin(),decision.licence_notices.end(),[](const auto& n){return n.manager.has_value();})==1,"human licence marker failed across group swaps");
    const auto& edge=decision.plan.groups.back().exchanges.front();const auto outgoing=edge.relegated,incoming=edge.promoted;
    auto other=s.world.membership[0].clubs[0];if(other==outgoing)other=s.world.membership[0].clubs[1];
    const auto borrowed=s.world.clubs.at(outgoing).roster[0].player,loaned=s.world.clubs.at(other).roster[0].player;
    s.world.loans={{borrowed,other,outgoing,5,0},{loaned,outgoing,other,7,1}};
    s.world.player_statistics[{outgoing,borrowed}]={{3,2,1},100,7,4};s.world.player_statistics[{incoming,loaned}]={{1,0,0},22,3,2};
    s.world.clubs.at(outgoing).fairplay_points=99;
    const auto bound=evaluate_annual_promotion_decisions(s,c);const auto source=s;
    const auto stage=stage_annual_membership(s,bound,text);
    check(stage.world.clubs.at(incoming).roster==s.world.clubs.at(outgoing).roster&&stage.world.clubs.at(outgoing).roster==std::array<RosterSlot,24>{},"newcomer did not inherit actual league-slot squad");
    check(stage.world.players==s.world.players&&stage.world.player_statistics==s.world.player_statistics,"amateur edge changed player identities or club statistics history");
    check(stage.world.loans[0].borrower==incoming&&stage.world.loans[1].owner==incoming&&stage.world.loans[0].source_slot==0,"amateur loans did not follow Source league slot");
    check(stage.world.clubs.at(incoming).fairplay_points==99&&stage.world.clubs.at(outgoing).fairplay_points==0,"league-slot discipline lineage changed");
    check(s==source&&before.world.players==s.world.players,"annual staging changed caller/player records");
    // Independent Source club swap oracle: roster/loan slot is retained,
    // while the statistics archive moves with the swapped club identity.
    auto original=club_swap_fixture();original.squad_slots[80*24]=10;original.player_statistics[(80u<<12)|10]={{3,2,1},100,7,4};original.player_statistics[(138u<<12)|20]={{1,0,0},22,3,2};
    original.memory.set32(layout::Career_RemoveJoinLoans_RuntimeField,(10u)|(0u<<12)|(80u<<22));
    original.memory.set32(layout::Career_RemoveJoinLoans_RuntimeField+4,(20u)|(80u<<12)|(0u<<22));
    original.memory.set16(layout::DeskRecords_Teams_RuntimeField+160,99);
    clubs_change_places(original,80,138);
    check(original.squad_slots[80*24]==10&&original.club_ids[80]==138&&original.club_ids[138]==80,"Source amateur squad identity oracle changed");
    check((original.memory.u32(layout::Career_RemoveJoinLoans_RuntimeField)>>22)==80&&((original.memory.u32(layout::Career_RemoveJoinLoans_RuntimeField+4)>>12)&1023)==80,"Source amateur loan indices unexpectedly remapped");
    check(original.player_statistics.at((138u<<12)|10).total==100&&original.player_statistics.at((80u<<12)|20).total==22,"Source archive did not follow club identity");
    auto excess=state_fixture();excess.game_seed=200000;
    for(uint8_t seat=0;seat<4;++seat){auto owned=manager(initial.plan.standings[0][size_t(6+seat)].club,{0},seat);owned.finance->balance=-7000000;excess.world.managers.push_back(owned);}
    const auto four=evaluate_annual_promotion_decisions(excess,context(excess,Country::germany));
    check(four.plan.groups[0].count==4&&four.plan.decisions.licences[0].withdrawn.size()==4,"Source extra human licences were truncated to nominal quota");
    auto promoted=state_fixture();const auto top=initial.plan.standings[1][0].club;promoted.world.managers.push_back(manager(top,{1}));
    const auto promotion=evaluate_annual_promotion_decisions(promoted,context(promoted,Country::germany));const auto human_stage=stage_annual_membership(promoted,promotion,text);
    check(human_stage.world.managers[0].career->season_services->promotion_tail_flags==0xb5,"promoted human eighteenth Source byte not marked");
    for(const auto& row:human_stage.world.managers[0].career->season_services->advertising_contracts)check(row[6]==0x80,"promoted human contract not marked");
    check(human_stage.world.economy->messages.size()==3&&human_stage.world.economy->messages[0].recipients==0x80000001u&&human_stage.world.economy->messages[1].recipients==1&&human_stage.world.economy->messages[2].recipients==0x80000001u,"licence/promotion mail lost Source group order");
    auto hidden=state_fixture();const auto english=evaluate_annual_promotion_decisions(hidden,context(hidden,Country::england));
    hidden.world.managers.push_back(manager(english.plan.standings[8].back().club,{8}));const auto hidden_before=hidden;
    const auto hidden_decision=evaluate_annual_promotion_decisions(hidden,context(hidden,Country::england));
    check(hidden_decision.plan.decisions.newcomers.size()==4&&hidden_decision.watcom_seed_after==english.watcom_seed_after&&
        hidden_decision.watcom_draws==english.watcom_draws&&
        std::count_if(hidden_decision.plan.groups.back().exchanges.begin(),hidden_decision.plan.groups.back().exchanges.end(),[](const auto& row){return row.protected_manager;})==1,
        "protected fourth-division manager skipped an actual paired newcomer draw");
    const auto protected_stage=stage_annual_membership(hidden,hidden_decision,text);
    check(protected_stage.world.clubs.at(hidden.world.managers[0].club).league==LeagueId{8}&&hidden==hidden_before,
        "Source lowest-visible human protection moved a manager or mutated the owner");
    check(hidden_decision.plan.groups.size()==4&&hidden_decision.plan.decisions.licences.size()==4&&
        protected_stage.world.membership[9]==hidden.world.membership[9],"English hidden division entered the Source promotion ladder");
    for(auto id:hidden.world.membership[9].clubs)check(protected_stage.world.clubs.at(id)==hidden.world.clubs.at(id),
        "English hidden roster/statistics lineage changed during visible promotion");
    auto legacy=state_fixture();legacy.world.managers.push_back(manager(legacy.world.membership[9].clubs[0],{9}));legacy.validate();
    check(read_save(make_save(legacy))==legacy,"readonly old hidden-division manager was globally rejected");
    auto unknown=promoted;unknown.world.managers[0].career->season_services->promotion_tail_flags.reset();const auto unsupported=evaluate_annual_promotion_decisions(unknown,context(unknown,Country::germany));
    rejects([&]{stage_annual_membership(unknown,unsupported,text);},"unknown old promotion tail guessed");
    auto pending=s;MarketListing listing;listing.player=borrowed;listing.seller=outgoing;pending.world.transfer_market->rows[0]=listing;
    const auto pending_decisions=evaluate_annual_promotion_decisions(pending,context(pending,Country::germany));
    rejects([&]{stage_annual_membership(pending,pending_decisions,text);},"unsettled annual transfer obligation waived");
    rejects([&]{stage_annual_membership(s,bound,TextDb{});},"missing actual annual message text accepted");
    auto stale=s;++stale.world.managers[0].finance->balance;rejects([&]{stage_annual_membership(stale,bound,text);},"changed post-account finances reused annual receipt");
    auto changed=bound;std::swap(changed.pool_after.slots[0],changed.pool_after.slots[1]);rejects([&]{stage_annual_membership(s,changed,text);},"changed pool order reused annual receipt");
    auto missing=s;missing.world.amateur_slots.reset();rejects([&]{evaluate_annual_promotion_decisions(missing,c);},"old unknown amateur order inferred from IDs");
    auto no_finances=s;no_finances.world.managers[0].career.reset();rejects([&]{evaluate_annual_promotion_decisions(no_finances,c);},"missing real licence operands accepted");
    // Repeated synthetic annual stages test persisted pool ordering, not played
    // seasons. Actual annual accounts/player reset/calendar remain unexecuted.
    auto next=state_fixture();for(int pass=0;pass<3;++pass) {
        const auto out=evaluate_annual_promotion_decisions(next,context(next,Country::england));const auto staged=stage_annual_membership(next,out,text);
        for(const auto& exchange:out.plan.groups.back().exchanges)if(!exchange.protected_manager) {
            const auto pos=std::find(out.pool_before.slots.begin(),out.pool_before.slots.end(),exchange.promoted)-out.pool_before.slots.begin();
            check(staged.amateurs.slots[size_t(pos)]==exchange.relegated,"amateur physical slot was resorted instead of replaced");
        }
        next.world=decode_world(encode_world(staged.world));next.game_seed=staged.game_seed;next.watcom_seed=staged.watcom_seed;++next.season;next.date+=365;
        check((*next.world.amateur_slots)[1]==staged.amateurs.slots,"schema36 lost later-year amateur order");
    }
}
}
int main(int argc,char** argv)try{set_fixture_tables(bmh::test::authored_fixture_tables());kernel_oracles();tail_capture();schema36();annual_models();if(argc==3)original_tail_fixture(argv[1],argv[2]);else if(argc!=1)throw std::runtime_error("usage: world12_annual_decisions_tests [Original data Original save]");std::cout<<"Source licence/newcomer RNG, eighteenth promotion byte, amateur roster/loan/statistics lineage and repeated synthetic pool stages passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
