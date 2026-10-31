// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "ui/world12_manager_selection.hpp"
#include "ui/flow.hpp"
#include "game/world12/codec.hpp"
#include "formats/palette.hpp"
#include "formats/data_profile.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;using namespace bmh::ui;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function> void rejects(Function function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,message);}
World fixture() {
    auto world=bmh::test::world12::complete_world();const auto old=world.membership[5].clubs[0];auto club=world.clubs.extract(old);
    const ClubId wide{0xf0000100u};club.key()=wide;club.mapped().id=wide;world.clubs.insert(std::move(club));world.membership[5].clubs[0]=wide;
    for(auto& [id,crest]:world.crests)crest={"W/0.VGA",0};world.validate();return world;
}
void builder_tests() {
    const auto world=fixture();const auto context=world12_manager_selection_context(world,71);
    check(context.world&&context.world->germany[0].clubs.size()==18&&context.world->england[0].clubs.size()==20&&context.world->england[4].clubs.size()==10,"country groups lost native league capacities");
    check(context.world->england[0].clubs[0].id.value==0xf0000100u,"picker narrowed global club IDs to signed legacy IDs");
    const auto fallback=select_world_scope(world,{true,false,true});const auto view=world12_manager_selection_context(fallback,72);
    check(!view.world->scope.england&&view.world->england[0].clubs.empty(),"fallback Premier League leaked into manager country choices");
    rejects([&]{world12_manager_selection_context(select_world_scope(world,{false,false,true}),73);},"club picker fabricated a national-only manager backend");
}
void save_image(const Image& image,const Palette& palette,const std::filesystem::path& path) {
    std::ofstream file(path,std::ios::binary);file<<"P6\n320 240\n255\n";
    for(auto p:image.pixels){const auto c=palette.argb[p];file.put(char(c>>16));file.put(char(c>>8));file.put(char(c));}
}
void ui_tests(const std::filesystem::path& data,const World& world,const std::filesystem::path& output,const std::filesystem::path& legacy_data) {
    const auto context=world12_manager_selection_context(world,71);ManagerScreen screen(data,0,{},context);
    const auto initial=screen.choice();check(initial.country==Country::germany&&initial.world_club==world.membership[0].clubs[0]&&initial.master_club==-1,"typed German initial club mismatch");
    const auto german=screen.render();check(screen.click(200,95),"country flag did not switch to England");
    const auto english=screen.render();check(screen.choice().country==Country::england&&screen.choice().world_club==world.membership[5].clubs[0]&&screen.choice().start_league==0,"England switch did not select first Premier League club");
    check(manager_json(screen.choice()).find(std::to_string(screen.choice().world_club.value))!=std::string::npos,"typed selection JSON lost wide club identity");
    check(screen.choice().name==initial.name&&screen.choice().portrait==initial.portrait&&screen.choice().strength==initial.strength,"country switch reset unrelated choices");
    rejects([&]{screen.master_of(0);},"typed club exposed as legacy selection ID");
    check(screen.click(252,95)&&screen.view()==ManagerView::club_picker,"new right-hand club logo did not open Original picker");
    const auto picker=screen.render();check(screen.group_last(0)-screen.group_first(0)+1==20,"English picker omitted clubs");
    check(screen.click(239,154)&&screen.choice().world_club==world.membership[5].clubs[19],"last English first-division logo is not selectable");
    screen.click(10,10,true);check(screen.view()==ManagerView::selection,"right click did not close club picker");
    check(screen.click(200,95)&&screen.choice().country==Country::germany&&screen.choice().world_club==world.membership[0].clubs[0],"country switch did not reset first German club");
    check(screen.render().pixels==german.pixels,"country roundtrip did not restore original German frame");
    ManagerChoice confirmed=initial;ManagerScreen second(data,1,{confirmed},context);
    check(second.choice().world_club==world.membership[0].clubs[1]&&second.choice().portrait!=confirmed.portrait,"second manager reused locked first club/portrait");
    second.click(200,95);check(second.choice().world_club==world.membership[5].clubs[0],"locked German club blocked English first club");
    const auto filtered=select_world_scope(world,{true,false,true});ManagerScreen de_only(data,0,{},world12_manager_selection_context(filtered,72));const auto locked=de_only.render();
    check(!de_only.click(200,95)&&de_only.render().pixels==locked.pixels,"disabled English scope became manager-playable via flag");
    const auto en_world=select_world_scope(world,{false,true,true});ManagerScreen en_only(data,0,{},world12_manager_selection_context(en_world,73));
    check(en_only.choice().country==Country::england&&en_only.choice().world_club==world.membership[5].clubs[0]&&!en_only.click(200,95),"English-only setup offered disabled Germany");
    auto bad=std::make_shared<ManagerWorldSelection>(*context.world);bad->england[0].clubs[0].crest_path="../bad.VGA";
    auto corrupt=context;corrupt.world=bad;ManagerScreen safe(data,0,{},corrupt);const auto safe_choice=safe.choice();const auto safe_frame=safe.render();
    rejects([&]{safe.click(200,95);},"bad country asset accepted");check(safe.choice()==safe_choice&&safe.render().pixels==safe_frame.pixels,"failed country switch partially changed visible selection");
    StartupFlow flow(data,{},SetupLeagueSymbol::country_selection);
    check(flow.click(10,10,true).request==FlowRequest::new_career&&flow.pending_career(),"typed setup did not request its selection owner");
    flow.begin_world_manager_selection(context);check(flow.screen_name()=="manager_selection","typed manager view did not start from setup");
    flow.click(200,95);flow.click(10,10,true);const auto result=flow.click(100,171);
    check(result.request==FlowRequest::new_career&&flow.pending_career()&&flow.pending_career()->managers.size()==1&&flow.pending_career()->managers[0].world_club==world.membership[5].clubs[0],"flow confirmation lost country/global club identity");
    StartupFlow multi(data,{},SetupLeagueSymbol::country_selection);
    const auto rules=load_reference_le(asset_path(data,"BMH.exe"),DataProfile{});
    multi.click(rules.u16(layout::Setup_FormatError_RuntimeField3+6*2)+10,rules.u8(layout::Setup_FormatError_RuntimeField4+6)+10);
    check(multi.options().managers==2,"manager-count fixture did not select two managers");
    multi.click(10,10,true);multi.begin_world_manager_selection(context);multi.click(200,95);multi.click(10,10,true);
    check(multi.click(100,171).request==FlowRequest::manager_confirmed,"first mixed-country manager did not wait for updated context");
    auto next=world12_manager_selection_context(world,72);multi.resume_manager_selection(next);multi.click(200,95);multi.click(10,10,true);
    check(multi.click(100,171).request==FlowRequest::new_career&&multi.pending_career()->managers[0].world_club==world.membership[5].clubs[0]&&multi.pending_career()->managers[1].world_club==world.membership[5].clubs[1],"next country chooser lost context or reused an occupied English club");
    if(!output.empty()) {
        std::filesystem::create_directories(output);const auto palette=load_palette(asset_path(data,"M3PC.COL"));
        save_image(german,palette,output/"germany.ppm");save_image(english,palette,output/"england.ppm");save_image(picker,palette,output/"england-picker.ppm");
        if(!legacy_data.empty()){ManagerScreen before(legacy_data,0);save_image(before.render(),palette,output/"legacy-before.ppm");}
        std::ofstream(output/"germany.json")<<manager_json(initial)<<'\n';
    }
}
}
int main(int argc,char** argv)try {
    builder_tests();
    if(argc>1){const auto world=argc>2?bmh::world12::decode_world(bmh::read_file(argv[2])):fixture();ui_tests(argv[1],world,argc>3?std::filesystem::path(argv[3]):std::filesystem::path{},argc>4?std::filesystem::path(argv[4]):std::filesystem::path{});}
    std::cout<<"Country manager selection: stable IDs, Original picker capacities, first-club reset, locking and disabled-English guards passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
