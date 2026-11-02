// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/settings_request_source.hpp"
#include "app/world12_save_load.hpp"
#include "game/world12/codec.hpp"
#include <chrono>
#include <iostream>
using namespace bmh;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,m);}
}
int main()try {
    const auto maximum=std::max({layout::Settings_ProjectSettingsAvailability_RuntimeField+7,
        layout::Settings_ProjectSettingsAvailability_RuntimeField2+7,layout::Settings_ProjectSettingsAvailability_RuntimeField3+7,
        layout::Settings_ProjectSettingsAvailability_RuntimeField4+256,layout::CareerSession_DispatchSettings_RuntimeField+20});
    auto rules=LeImage::from_bytes(0,Bytes(maximum+1));
    for(uint32_t row=0;row<20;++row){const uint8_t limit=5;rules.patch(layout::CareerSession_DispatchSettings_RuntimeField+row,&limit,1);}
    GameState g;g.manager_teams={0};g.memory.add_block(layout::LeaguePresentationOptions,180);g.memory.add_block(layout::ActiveManager,1);
    g.memory.add_block(layout::GameMode,1);g.memory.set8(layout::GameMode,3);g.memory.add_block(layout::CompetitionSelection,4);g.memory.set32(layout::CompetitionSelection,3);
    uint64_t revision=9;std::string error;
    auto request=[&](uint8_t row,uint8_t col,ui::SettingsEdit edit){ui::SettingsWriteRequest r;r.manager_slot=0;r.revision=revision;r.row=row;r.column=col;r.operation=edit;for(uint32_t i=0;i<180;++i)r.expected_raw[i]=g.memory.u8(layout::LeaguePresentationOptions+i);return r;};
    auto toggle=request(3,5,ui::SettingsEdit::toggle_binary);
    check(app::apply_settings_request_source(g,rules,std::nullopt,true,toggle,revision,error)&&revision==10&&g.memory.u8(layout::LeaguePresentationOptions+32)==1,"shared Source raster toggle failed");
    check(!app::apply_settings_request_source(g,rules,std::nullopt,true,toggle,revision,error)&&revision==10,"stale settings replay changed revision");
    auto sound=request(0,5,ui::SettingsEdit::toggle_binary);check(!app::apply_settings_request_source(g,rules,std::nullopt,true,sound,revision,error),"unknown sound capability accepted");
    check(!app::apply_settings_request_source(g,rules,ui::SettingsSoundCapabilities{false,false},true,sound,revision,error),"missing sound device accepted");
    check(app::apply_settings_request_source(g,rules,ui::SettingsSoundCapabilities{true,true},true,sound,revision,error),"available sound control rejected");
    g.memory.set8(layout::LeaguePresentationOptions+8,3);auto odd=request(0,8,ui::SettingsEdit::decrement);
    check(app::apply_settings_request_source(g,rules,std::nullopt,true,odd,revision,error)&&g.memory.u8(layout::LeaguePresentationOptions+8)==1,"odd numeric speed was normalized");
    odd=request(0,8,ui::SettingsEdit::decrement);check(!app::apply_settings_request_source(g,rules,std::nullopt,true,odd,revision,error),"numeric minimum underflow accepted");
    g.memory.set8(layout::LeaguePresentationOptions+7,5);auto max=request(0,7,ui::SettingsEdit::increment);check(!app::apply_settings_request_source(g,rules,std::nullopt,true,max,revision,error),"Source numeric limit bypassed");
    TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic calendar refusal");g.texts=&texts;g.season_start=365;
    for(uint32_t day:{109,110,230,231}) {
        g.date=365+day;g.memory.set8(layout::LeaguePresentationOptions+16*9+4,0);const auto before_revision=revision;
        auto indoor=request(16,4,ui::SettingsEdit::toggle_binary);
        const bool changed=app::apply_settings_request_source(g,rules,std::nullopt,true,indoor,revision,error);
        check(changed==(day<110||day>230),"Source indoor date window lost inclusive seasonal limits");
        check(revision==before_revision+unsigned(changed)&&g.memory.u8(layout::LeaguePresentationOptions+16*9+4)==unsigned(changed),"refused Source date changed settings/revision");
    }
    g.texts=nullptr;
    auto world=bmh::test::world12::complete_world();world.players_initialized=true;world.markets.emplace();world.free_players.emplace();world.settings=bmh::world12::CareerSettings{3,3,5,1,false};world.economy.emplace();
    auto& matrix=world.economy->presentation_options.emplace();for(size_t i=0;i<180;++i)matrix[i]=uint8_t(i*17);
    check(bmh::world12::decode_world(bmh::world12::encode_world(world))==world,"presentation options lost raw bits in codec roundtrip");
    const auto serialized=bmh::world12::encode_world(world);auto root=cli::parse_json(std::string(serialized.begin(),serialized.end()));
    root.object()->at("version")=cli::Json(30);auto encoded=cli::dump_json(root);
    rejects([&]{bmh::world12::decode_world(Bytes(encoded.begin(),encoded.end()));},"schema30 admitted settings authority it never saved");
    root.object()->at("economy").object()->erase("presentation_options");encoded=cli::dump_json(root);
    check(!bmh::world12::decode_world(Bytes(encoded.begin(),encoded.end())).economy->presentation_options,"old settings were fabricated from current reference");
    const auto directory=std::filesystem::temp_directory_path()/("openbmh-EIN-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);}} cleanup{directory};
    ui::SaveLoadRequest file;file.action=ui::SaveLoadAction::save;file.file_kind=ui::SaveLoadFileKind::settings;file.origin_item_id=19;file.path=std::filesystem::absolute(directory/"Test.EIN");
    const auto path=bmh::app::world12::checked_settings_path(directory,file);check(path==std::filesystem::canonical(directory)/"Test.EIN","canonical EIN confinement rejected");
    const Bytes bytes(matrix.begin(),matrix.end());bmh::app::world12::publish_settings(path,bytes,false);check(read_file(path)==bytes,"EIN did not preserve raw 180 bytes");
    rejects([&]{bmh::app::world12::publish_settings(path,Bytes(179),true);},"short EIN replaced existing file");check(read_file(path)==bytes,"failed EIN publish damaged old file");
    file.path=std::filesystem::absolute(directory.parent_path()/"Other.EIN");rejects([&]{bmh::app::world12::checked_settings_path(directory,file);},"EIN escaped save directory");
    std::cout<<"Shared settings guards, sound capabilities, numeric bounds, raw schema31, unknown schema30 and atomic EIN publication passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
