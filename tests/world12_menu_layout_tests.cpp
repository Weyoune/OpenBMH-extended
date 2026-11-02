// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_save_load.hpp"
#include "app/menu_layout_source.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;namespace files=bmh::app::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);}
}
int main()try {
    const auto directory=std::filesystem::temp_directory_path()/("openbmh-MEN-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{directory};
    Bytes bytes(864,0);bytes[5]=1;bytes[16]=20;bytes[18]=20;bytes[19]=32;bytes[20]=25;bytes[28]=19;bytes[53*16+12]=72;
    std::string error;check(app::validate_menu_layout_source(bytes,error),"synthetic native layout rejected");
    auto invalid=bytes;invalid.pop_back();check(!app::validate_menu_layout_source(invalid,error),"short MEN accepted");
    invalid=bytes;invalid[5]=54;check(!app::validate_menu_layout_source(invalid,error),"invalid head accepted");
    invalid=bytes;invalid[16+5]=1;check(!app::validate_menu_layout_source(invalid,error),"cyclic layout accepted");
    auto custom=bytes;custom[16+6]=1;check(app::validate_menu_layout_source(custom,error),"valid custom backlink normalized or rejected");
    auto empty=bytes;empty[5]=0;check(app::validate_menu_layout_source(empty,error),"Source empty layout rejected");
    const auto path=std::filesystem::absolute(directory/"View.MEN");ui::SaveLoadRequest request;request.action=ui::SaveLoadAction::save;request.file_kind=ui::SaveLoadFileKind::menu_layout;request.origin_item_id=53;request.path=path;
    // macOS temporary directories can be reached through /var -> /private/var.
    const auto canonical_path=std::filesystem::canonical(directory)/path.filename();
    check(files::checked_menu_layout_path(directory,request)==canonical_path,"confined MEN path rejected");files::publish_menu_layout(path,bytes,false);check(read_file(path)==bytes,"published MEN differs from exact source payload");
    rejects([&]{files::checked_menu_layout_path(directory,request);},"existing MEN accepted without overwrite confirmation");
    rejects([&]{files::publish_menu_layout(path,custom,false);},"exclusive publication replaced an existing MEN");check(read_file(path)==bytes,"failed exclusive MEN publish changed previous target");
    files::publish_menu_layout(path,custom,true);check(read_file(path)==custom,"confirmed overwrite lost custom source bytes");
    rejects([&]{files::publish_menu_layout(path,invalid,true);},"malformed MEN published");check(read_file(path)==custom,"malformed overwrite destroyed old MEN");
    request.overwrite=true;check(files::checked_menu_layout_path(directory,request)==canonical_path,"confirmed regular MEN overwrite rejected");
    auto outside=request;outside.path=directory.parent_path()/"Other.MEN";rejects([&]{files::checked_menu_layout_path(directory,outside);},"MEN escaped configured root");
    auto wrong=request;wrong.origin_item_id=52;rejects([&]{files::checked_menu_layout_path(directory,wrong);},"wrong source handle accepted");wrong=request;wrong.path=directory/"View.FBM";rejects([&]{files::checked_menu_layout_path(directory,wrong);},"FBM reached MEN loader");
    const auto blocked=directory/"Blocked.MEN";std::filesystem::create_directory(blocked);rejects([&]{files::publish_menu_layout(blocked,bytes,true);},"MEN publication replaced a directory");check(std::filesystem::is_directory(blocked),"failed MEN publication removed a directory");
    std::error_code link_error;std::filesystem::create_symlink(path,directory/"Link.MEN",link_error);
    if(!link_error){auto symlink=request;symlink.path=directory/"Link.MEN";symlink.action=ui::SaveLoadAction::load;symlink.overwrite=false;rejects([&]{files::checked_menu_layout_path(directory,symlink);},"symbolic MEN load accepted");}
    CareerSnapshot state;state.world=bmh::test::world12::complete_world();state.results=SeasonResults::empty(state.world.catalogue);const auto save=make_save(state);
    files::publish_game_save(directory/"Game.FBM",save,false);check(read_save(load_savegame(directory/"Game.FBM"))==state,"shared byte publisher regressed game-save verification");
    for(const auto& entry:std::filesystem::directory_iterator(directory))check(entry.path().extension()!=".tmp","failed publication leaked a temporary file");
    std::cout<<"MEN source structure, exact bytes, custom links, confined paths, overwrite and failed-publication preservation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
