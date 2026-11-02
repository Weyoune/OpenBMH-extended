// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_save_load.hpp"
#include "app/menu_layout_source.hpp"
#include "game/save.hpp"
#include "game/calendar.hpp"
#include "game/world12/desktop_projection.hpp"
#include "ui/desk_state.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <limits>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace bmh::app::world12 {
namespace {
void io_error(const char* operation){throw std::system_error(errno,std::generic_category(),operation);}
ui::SetupOptions restored_options(const bmh::world12::CareerSnapshot& state){
    if(!state.world.settings)throw FormatError("world12: saved career settings are missing");const auto s=*state.world.settings;const auto scope=state.world.catalogue.scope;
    ui::SetupOptions o;o.duration=s.duration;o.difficulty=s.difficulty;o.managers=s.planned_managers;o.mode=s.mode;o.league=scope.club_play();o.world=scope.international;o.country_selection=true;o.germany=scope.germany;o.england=scope.england;return o;
}
}
Session::Session(RuntimeResources resources,bmh::world12::CareerSnapshot state,uint64_t scope,uint64_t revision,ResumeTag)
    :resources_(std::move(resources)),options_(restored_options(state)),state_(std::move(state)),owner_{scope,revision} {
    state_.validate();
    const bool desk_owned=state_.desktop&&state_.desktop->initial_event&&state_.desktop->active_seat;
    const bool clock_owned=state_.progress&&state_.desktop&&!state_.desktop->initial_event&&!state_.desktop->active_seat;
    if(!scope||!revision||revision==std::numeric_limits<uint64_t>::max()||(!desk_owned&&!clock_owned)||!state_.calendar||!state_.world.transfer_market||!state_.world.initial_competitions||!state_.world.initial_competitions->manager_services_complete||state_.world.managers.size()!=size_t(options_.managers))
        throw FormatError("world12: snapshot has no resumable event/progression owner");
    if(clock_owned){const auto epoch=calendar_epoch_from_executable(resources_.german);if(state_.progress->epoch_year!=epoch.year||state_.progress->epoch_weekday!=epoch.weekday)throw FormatError("world12: saved clock requires another calendar epoch");}
    for(const auto& m:state_.world.managers)if(!m.finance||!m.tactics||!m.tactics->backing||!m.match_state||!m.career||!m.career->season_services)throw FormatError("world12: saved manager state is incomplete");
    for(const auto& stage:state_.cups)if(auto notice=bmh::world12::cup_draw_notice(state_.world,stage))draws_.domestic_draws.push_back(std::move(*notice));
    for(const auto& m:state_.world.managers){const auto& tickets=*m.career->season_services;services_.tickets.push_back({m.seat,tickets.tickets_sold,tickets.ticket_income});}
}
Session Session::resume(RuntimeResources resources,bmh::world12::CareerSnapshot state,uint64_t scope,uint64_t revision){return Session(std::move(resources),std::move(state),scope,revision,ResumeTag{});}
void Session::validate_file_owner(int manager_slot,uint64_t revision)const {
    if(preparation_menu_||revision!=owner_.revision||owner_.revision==std::numeric_limits<uint64_t>::max())throw FormatError("world12: stale or unavailable file owner");
    const auto local=bmh::world12::project_manager_desktop(state_,resources_.reference);
    if(manager_slot!=local.state.memory.u8(layout::ActiveManager))throw FormatError("world12: file request belongs to another manager");
}
Savegame Session::prepare_manual_save(const ui::SaveLoadRequest& request){
    validate_file_owner(request.manager_slot,request.revision);
    if(request.file_kind!=ui::SaveLoadFileKind::game||request.action!=ui::SaveLoadAction::save||request.origin_item_id!=-1||request.revision!=owner_.revision||owner_.revision==std::numeric_limits<uint64_t>::max()||!state_.desktop||!state_.desktop->active_seat)
        throw FormatError("world12: stale or unavailable save owner");
    const auto before=state_;const auto revision=owner_.revision;auto candidate=state_;WatcomRandom random(candidate.watcom_seed);
    const uint32_t high=uint32_t(random.range(0,0x8fff)),low=uint32_t(random.range(0,0x8fff));
    const auto key=uint8_t(random.range(0,255)+'n'),byte=uint8_t(random.range(0,255));
    candidate.watcom_seed=random.seed();candidate.desktop->autosave_pending=false;
    auto save=bmh::world12::make_save(candidate);save.id2=low+high*0x10000u+1;save.key=key;save.random=byte;save.checksum=key;for(auto b:save.stream)save.checksum=uint8_t(save.checksum+b);
    if(state_!=before||owner_.revision!=revision)throw FormatError("world12: save owner changed during preparation");
    state_=std::move(candidate);++owner_.revision;return save;
}
namespace {
std::filesystem::path checked_file_path(const std::filesystem::path& directory,const ui::SaveLoadRequest& request,
    ui::SaveLoadFileKind kind,const char* suffix) {
    if(request.file_kind!=kind||!request.path.is_absolute()||request.path.filename().empty()||
       (kind==ui::SaveLoadFileKind::game?request.origin_item_id!=-1:request.origin_item_id!=53))throw FormatError("world12: invalid file request owner");
    auto extension=request.path.extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(extension!=suffix)throw FormatError("world12: unexpected saved-file extension");
    const auto root=std::filesystem::canonical(directory);const auto parent=std::filesystem::canonical(request.path.parent_path());
    if(!std::filesystem::equivalent(root,parent))throw FormatError("world12: game file is outside the configured save directory");
    const auto target=parent/request.path.filename();std::error_code ec;const auto status=std::filesystem::symlink_status(target,ec);
    if(ec&&ec!=std::errc::no_such_file_or_directory)throw std::system_error(ec,"world12: game file unavailable");
    const bool exists=!ec&&std::filesystem::exists(status);
    if(exists&&!std::filesystem::is_regular_file(status))throw FormatError("world12: game file is not a regular file");
    if(request.action==ui::SaveLoadAction::load&&(!exists||request.overwrite))throw FormatError("world12: invalid load target");
    if(request.action==ui::SaveLoadAction::save&&exists&&!request.overwrite)throw FormatError("world12: overwrite confirmation required");
    return target;
}
}
std::filesystem::path checked_game_path(const std::filesystem::path& directory,const ui::SaveLoadRequest& request){return checked_file_path(directory,request,ui::SaveLoadFileKind::game,".fbm");}
std::filesystem::path checked_menu_layout_path(const std::filesystem::path& directory,const ui::SaveLoadRequest& request){return checked_file_path(directory,request,ui::SaveLoadFileKind::menu_layout,".men");}

namespace {
void publish_checked_bytes(const std::filesystem::path& target,const Bytes& bytes,bool overwrite,
    const std::function<void(const std::filesystem::path&)>& verify) {
    static std::atomic<uint64_t> serial{0};std::filesystem::path temporary;
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE;
    for(int n=0;n<100&&file==INVALID_HANDLE_VALUE;++n){temporary=target.parent_path()/(L".openbmh-save-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial)+L".tmp");file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE&&GetLastError()!=ERROR_FILE_EXISTS)throw std::system_error(int(GetLastError()),std::system_category(),"create save temporary");}
    if(file==INVALID_HANDLE_VALUE)throw FormatError("world12: no exclusive save temporary available");
#else
    int file=-1;
    for(int n=0;n<100&&file<0;++n){temporary=target.parent_path()/(".openbmh-save-"+std::to_string(getpid())+"-"+std::to_string(++serial)+".tmp");file=::open(temporary.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);if(file<0&&errno!=EEXIST)io_error("create save temporary");}
    if(file<0)throw FormatError("world12: no exclusive save temporary available");
#endif
    try {
#ifdef _WIN32
        size_t done=0;while(done<bytes.size()){DWORD wrote=0;if(!WriteFile(file,bytes.data()+done,DWORD(std::min<size_t>(bytes.size()-done,1u<<20)),&wrote,nullptr)||!wrote)throw std::system_error(int(GetLastError()),std::system_category(),"write save temporary");done+=wrote;}
        if(!FlushFileBuffers(file))throw std::system_error(int(GetLastError()),std::system_category(),"flush save temporary");CloseHandle(file);file=INVALID_HANDLE_VALUE;
#else
        size_t done=0;while(done<bytes.size()){const auto wrote=::write(file,bytes.data()+done,bytes.size()-done);if(wrote<0&&errno==EINTR)continue;if(wrote<=0)io_error("write save temporary");done+=size_t(wrote);}
        if(::fsync(file))io_error("flush save temporary");if(::close(file)){file=-1;io_error("close save temporary");}file=-1;
#endif
        if(read_file(temporary)!=bytes)throw FormatError("world12: temporary file verification differs");
        verify(temporary);
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH|(overwrite?MOVEFILE_REPLACE_EXISTING:0)))throw std::system_error(int(GetLastError()),std::system_category(),"publish game save");
#else
        if(overwrite){if(::rename(temporary.c_str(),target.c_str()))io_error("replace game save");}
        else {if(::link(temporary.c_str(),target.c_str()))io_error("publish new game save");std::error_code ignored;std::filesystem::remove(temporary,ignored);}
#endif
    }catch(...){
#ifdef _WIN32
        if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
#else
        if(file>=0)::close(file);
#endif
        std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;
    }
}
}
void publish_game_save(const std::filesystem::path& target,const Savegame& save,bool overwrite) {
    const auto expected=bmh::world12::read_save(save);const auto bytes=savegame_bytes(save);
    publish_checked_bytes(target,bytes,overwrite,[&](const auto& path){if(bmh::world12::read_save(load_savegame(path))!=expected)throw FormatError("world12: temporary save verification differs");});
}
void publish_menu_layout(const std::filesystem::path& target,const Bytes& bytes,bool overwrite) {
    std::string error;if(!validate_menu_layout_source(bytes,error))throw FormatError(error);
    publish_checked_bytes(target,bytes,overwrite,[](const auto& path){std::string error;if(!validate_menu_layout_source(read_file(path),error))throw FormatError(error);});
}

ui::SaveLoadInfoReader save_info_reader(LeImage rules){return [rules=std::move(rules)](const std::filesystem::path& path){
    const auto state=bmh::world12::read_save(load_savegame(path));const auto date=ui::desk_date(state.date,rules);
    ui::SaveLoadInfo info;const auto filename=path.stem().u8string();info.path.assign(filename.begin(),filename.end());const auto two=[](int n){return (n<10?"0":"")+std::to_string(n);};info.date=two(date.day)+"."+two(date.month)+"."+std::to_string(date.year);
    for(const auto& m:state.world.managers){const auto& club=state.world.clubs.at(m.club);info.rows.push_back({ui::original_to_utf8(m.name),ui::original_to_utf8(club.name),state.world.catalogue.league(club.league).name});}return info;
};}
}
