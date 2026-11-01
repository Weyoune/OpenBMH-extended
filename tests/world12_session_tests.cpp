// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_startup.hpp"
#include "app/world12_desktop.hpp"
#include "game/world12/match_projection.hpp"
#include "game/desk.hpp"
#include "game/world12/codec.hpp"
#include "game/save.hpp"
#include "formats/data_profile.hpp"
#include "formats/palette.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;namespace session_app=bmh::app::world12;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,m);}
session_app::RuntimeResources synthetic() {
    session_app::RuntimeResources r;r.master=bmh::test::world12::complete_world();
    for(uint32_t i=0;i<54;++i){PlayerRecord p;p.id={1000000+i};p.attributes={"Extra",50u|(60u<<7)|(70u<<14),0,i%4};r.master.players[p.id]=p;}
    auto image=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));const auto byte=[&](uint32_t at,uint8_t v){image.patch(at,&v,1);};
    for(uint32_t i=0;i<9;++i){byte(layout::LeagueStrengthRanges+i*2,25);byte(layout::LeagueStrengthRanges+i*2+1,75);}
    for(uint32_t league=0;league<4;++league){byte(layout::Career_BuildStadium_RuntimeField4+league,5);for(uint32_t i=0;i<22;++i)byte(layout::Career_BuildStadium_RuntimeField+league*23+i,1);}
    for(uint32_t i=0;i<22;++i){byte(layout::Career_BuildStadium_RuntimeField2+i*5,2);byte(layout::Career_BuildStadium_RuntimeField3+i*5,5);}
    r.german=image;r.english=image;r.reference.stocks.resize(12*0x5c);r.reference.memory.add_block(layout::FundBlock,6*0x1c);r.reference.formations.resize(0x580);r.reference.manager_records.resize(5*0xb0c);r.reference.tactics.resize(4*0x840);r.reference.manager_options.resize(20);r.reference.manager_ratings.resize(240);
    const uint8_t depths[]={0,5,5,5,5,10,10,10,10,17,17};for(size_t m=0;m<4;++m){r.reference.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<8;++f)for(size_t row=0;row<11;++row)r.reference.formations[m*0x160+mode*0xb0+f*22+row]=depths[row];}
    r.reference.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);return r;
}
session_app::ManagerActionOwner* interrupted_desktop_owner=nullptr;
void interrupt_desktop(int){if(interrupted_desktop_owner){++interrupted_desktop_owner->revision;interrupted_desktop_owner=nullptr;}}
void desktop_tests(CareerSnapshot prepared,const GameState& source,const std::filesystem::path& output={}) {
    auto reference=source;reference.memory.add_block(layout::Desktops,4*0x360);
    reference.memory.add_block(layout::CareerDuration,1);
    reference.club_ids.resize(445);reference.team_flags.resize(445);reference.team_morale.resize(445);
    if(!prepared.desktop)session_app::initialize_desktops(prepared,reference);
    const auto before=prepared;
    for(const auto& manager:before.world.managers){
        auto state=before;session_app::ManagerActionOwner owner{77,8};const auto settings=*state.world.settings;
        std::vector<ClubId> clubs;for(const auto& m:state.world.managers)clubs.push_back(m.club);
        auto oracle=AiMatchProjection::create_for_humans(state,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        size_t local=0;while(oracle.global_manager(uint8_t(local))!=manager.seat)++local;
        oracle.state.memory.set8(layout::CareerDuration,settings.duration);
        GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);bool autosave=false;
        bmh::enter_desk(oracle.state,int(local),game,random,[&](const GameState&,WatcomRandom&){autosave=true;});
        const auto outcome=session_app::enter_initial_manager_desktop(state,owner,{77,8},manager.seat,reference);
        check(owner.revision==9&&state.game_seed==game.seed()&&state.watcom_seed==random.seed()&&outcome.game_draws==game.calls()&&outcome.watcom_draws==random.calls(),"desktop entry lost Original random stream/owner");
        const auto& runtime=*state.desktop;const auto& desk=*std::find_if(runtime.managers.begin(),runtime.managers.end(),[&](const auto& d){return d.seat==manager.seat;});
        check(runtime.active_seat==manager.seat&&desk.prestige==oracle.state.manager_records[local*0xb0c+0x211]&&runtime.autosave_month==oracle.state.autosave_month&&runtime.autosave_months==oracle.state.autosave_months&&runtime.autosave_pending==autosave&&outcome.autosave_due==autosave,"desktop lost prestige/autosave/active manager");
        for(size_t r=0;r<54;++r)for(size_t b=0;b<16;++b){auto expected=oracle.state.memory.u8(layout::Desktops+uint32_t(local*0x360+r*16+b));if(state.world.manager_country(manager.seat)==Country::england&&r==10&&b==1)expected|=0x10;check(desk.layout[r][b]==expected,"desktop layout differs from Original country policy");}
        check(state.date==before.date&&state.calendar==before.calendar&&state.results==before.results&&state.world==before.world,"desktop entry advanced time/results or mutated unrelated world state");
        check(read_save(make_save(state))==state,"entered desktop did not survive save roundtrip");
        if(!output.empty()){
            std::filesystem::create_directories(output);
            const auto path=output/("desk-seat-"+std::to_string(manager.seat)+".FBM");write_savegame(path,make_save(state));
            check(read_save(load_savegame(path))==state,"entered desktop on-disk roundtrip differs");
            std::ofstream receipt(output/("desk-seat-"+std::to_string(manager.seat)+".txt"));
            receipt<<"seat="<<int(manager.seat)<<" country="<<int(state.world.manager_country(manager.seat))<<" prestige="<<int(desk.prestige)
                <<" G="<<state.game_seed<<" W="<<state.watcom_seed<<" Gdraws="<<outcome.game_draws<<" Wdraws="<<outcome.watcom_draws<<" date="<<state.date<<'\n';
        }
        const auto entered=state;rejects([&]{session_app::enter_initial_manager_desktop(state,owner,{77,8},manager.seat,reference);},"stale desktop entry accepted");
        rejects([&]{session_app::enter_initial_manager_desktop(state,owner,owner,manager.seat,reference);},"initial desktop entry replay accepted");check(state==entered&&owner.revision==9,"rejected desktop entry changed state");
    }
    {auto interrupted=before;session_app::ManagerActionOwner owner{77,8};interrupted_desktop_owner=&owner;draw_hook=interrupt_desktop;
        rejects([&]{session_app::enter_initial_manager_desktop(interrupted,owner,{77,8},before.world.managers.front().seat,reference);},"desktop overwrote externally updated owner");draw_hook=nullptr;interrupted_desktop_owner=nullptr;
        check(interrupted==before&&owner.revision==9,"desktop rollback lost external revision or leaked staged state");}
    {auto pending=before;auto automatic=reference;automatic.memory.add_block(layout::LeaguePresentationOptions,0x62);automatic.memory.set8(layout::Desk_EnterDesk_RuntimeField,1);
        session_app::ManagerActionOwner owner{77,8};const auto outcome=session_app::enter_initial_manager_desktop(pending,owner,owner,0,automatic);
        check(outcome.autosave_due&&pending.desktop->autosave_pending&&pending.desktop->autosave_month==7&&pending.desktop->autosave_months==0,"initial monthly autosave was not retained for file owner");
        check(read_save(make_save(pending))==pending,"pending autosave lost on reload");}
    auto invalid=before;invalid.desktop->managers.push_back(invalid.desktop->managers.front());rejects([&]{make_save(invalid);},"duplicate saved desktop accepted");
    invalid=before;invalid.desktop->active_seat=4;rejects([&]{make_save(invalid);},"unknown active desktop accepted");
    auto truncated=make_save(before);truncated.stream.pop_back();rejects([&]{read_save(truncated);},"truncated desktop snapshot accepted");
    auto old=before;old.desktop.reset();auto legacy=make_save(old);legacy.stream[7]='4';legacy.stream.pop_back();legacy.checksum=0;for(auto b:legacy.stream)legacy.checksum=uint8_t(legacy.checksum+b);check(read_save(legacy)==old,"previous S4 snapshot ceased to load");
}
void actual(const std::filesystem::path& data,const std::filesystem::path& output) {
    auto resources=session_app::load_runtime_resources(data);
    check(resources.reference.manager_teams.empty(),"reference retained invented manager ownership");
    for(auto slot:resources.reference.squad_slots)check((slot&0xfff)==0xfff,"data-only reference imported actual packed players");
    ui::StartupFlow flow(data,{},ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> session;
    const auto begin=flow.click(10,10,true);auto result=session_app::serve_startup(flow,begin,session,data,91,71);
    check(result.error.empty()&&result.progress==session_app::StartupProgress::selecting&&session&&flow.screen_name()=="manager_selection","actual setup did not create typed manager session");
    flow.click(200,95);flow.click(252,95);flow.click(239,154);flow.click(10,10,true);
    if(!output.empty()){std::filesystem::create_directories(output);const auto frame=flow.render();const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));std::ofstream f(output/"english-selection.ppm",std::ios::binary);f<<"P6\n320 240\n255\n";for(auto p:frame.pixels){const auto c=palette.argb[p];f.put(char(c>>16));f.put(char(c>>8));f.put(char(c));}}
    flow.click(10,10,true);const auto accepted=flow.click(100,171);result=session_app::serve_startup(flow,accepted,session,data,91,71);
    check(result.error.empty()&&result.progress==session_app::StartupProgress::prepared&&session->prepared(),"actual chooser confirmation did not prepare typed session");
    check(session->snapshot().world.manager_country(0)==Country::england&&session->snapshot().world.transfer_market,"actual country/club choice lost during complete start");
    check(read_save(make_save(session->snapshot()))==session->snapshot(),"actual session snapshot lost data");
    if(!output.empty()){write_savegame(output/"prepared-start.FBM",make_save(session->snapshot()));check(read_save(load_savegame(output/"prepared-start.FBM"))==session->snapshot(),"actual on-disk typed snapshot differs");const auto world=encode_world(session->snapshot().world);std::ofstream f(output/"prepared-world.json",std::ios::binary);f.write(reinterpret_cast<const char*>(world.data()),std::streamsize(world.size()));}
    desktop_tests(session->snapshot(),resources.reference,output.empty()?output:output/"single");
    ui::StartupFlow mixed(data,{},ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> multi;
    mixed.click(resources.german.u16(layout::Setup_FormatError_RuntimeField3+6*2)+10,resources.german.u8(layout::Setup_FormatError_RuntimeField4+6)+10);
    check(mixed.options().managers==2,"actual setup failed to choose two managers");
    auto selected=session_app::serve_startup(mixed,mixed.click(10,10,true),multi,data,91,72);check(selected.error.empty(),"actual mixed session did not start");
    mixed.click(200,95);mixed.click(10,10,true);selected=session_app::serve_startup(mixed,mixed.click(100,171),multi,data,91,72);
    check(selected.error.empty()&&selected.progress==session_app::StartupProgress::selecting&&multi->snapshot().world.managers.size()==1,"actual first manager confirmation did not resume newer chooser context");
    mixed.click(10,10,true);selected=session_app::serve_startup(mixed,mixed.click(100,171),multi,data,91,72);
    check(selected.error.empty()&&selected.progress==session_app::StartupProgress::prepared&&multi->snapshot().world.manager_country(0)==Country::england&&multi->snapshot().world.manager_country(1)==Country::germany,"actual mixed manager country ownership lost across GUI confirmations");
    desktop_tests(multi->snapshot(),resources.reference,output.empty()?output:output/"mixed");
    std::cout<<"Actual standalone data-only package: original setup/England picker -> typed confirmation -> all start services/market/snapshot passed; desktop/day owner still pending\n";
}
}
int main(int argc,char** argv)try {
    const auto resources=synthetic();
    for(int mask=1;mask<8;++mask){const GameScope scope{bool(mask&1),bool(mask&2),bool(mask&4)};if(!scope.club_play())continue;
        ui::SetupOptions options;options.country_selection=true;options.germany=scope.germany;options.england=scope.england;options.world=scope.international;options.league=true;options.managers=int(scope.germany)+int(scope.england);
        session_app::Session session(resources,options,91,71);const auto before=session.snapshot();const auto rev=session.revision();
        rejects([&]{session.prepare_start(rev);},"session allowed start without selected managers");check(session.snapshot()==before&&session.revision()==rev,"failed start leaked session state");
        for(auto country:{Country::germany,Country::england})if(scope.domestic_enabled(country)){const auto context=session.selection_context();const auto league=session.snapshot().world.catalogue.starting_league(country,0);ui::ManagerChoice choice;choice.slot=int(session.snapshot().world.managers.size());choice.master_club=-1;choice.world_club=session.snapshot().world.membership[league.value].clubs[0];choice.country=country;choice.name="Session "+std::to_string(choice.slot);choice.portrait=choice.slot;choice.roster_revision=context.revision;
            session.confirm_manager(choice,context.revision);check(session.snapshot().world.managers.back().club==choice.world_club&&session.revision()==context.revision+1,"session chooser lost stable identity/revision");const auto accepted=session.snapshot();rejects([&]{session.confirm_manager(choice,context.revision);},"session replayed accepted chooser result");check(session.snapshot()==accepted,"replayed manager changed session");}
        check(read_save(make_save(session.snapshot()))==session.snapshot(),"pending session snapshot lost state");
        desktop_tests(session.snapshot(),resources.reference);
    }
    if(argc>1)actual(argv[1],argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});
    std::cout<<"Six session scopes, owned chooser revisions, Original desktop entry/RNG/country policy, S4/S5 saves and rollback passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
