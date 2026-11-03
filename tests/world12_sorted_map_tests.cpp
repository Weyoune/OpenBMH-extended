// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/sorted_map.hpp"
#include "game/world12/annual_archive_codec.hpp"
#include "game/world12/regional_playoff_codec.hpp"
#include "game/world12/save.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <new>
#include <type_traits>

namespace {
bool reject_allocations=false;
}
void* operator new(std::size_t size) {
    if(reject_allocations)throw std::bad_alloc();
    if(auto* memory=std::malloc(size?size:1))return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* memory)noexcept{std::free(memory);}
void operator delete[](void* memory)noexcept{std::free(memory);}
void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}
void operator delete[](void* memory,std::size_t)noexcept{std::free(memory);}

namespace {
using namespace bmh;
using namespace bmh::world12;
static_assert(std::is_nothrow_move_constructible_v<NationalCampaignHistory>);
static_assert(std::is_nothrow_move_constructible_v<AnnualTitleArchive>);
static_assert(std::is_nothrow_move_constructible_v<RegionalPlayoffSeason>);
static_assert(std::is_nothrow_move_constructible_v<FriendlyPlan>);
static_assert(std::is_nothrow_move_assignable_v<World>);
static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);
static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<SortedMap<uint32_t,int>::iterator>()->first)>>);
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<class F>void rejects(F function,const char* reason){bool rejected=false;try{function();}catch(...){rejected=true;}check(rejected,reason);}

void map_oracle() {
    SortedMap<uint32_t,int> actual;std::map<uint32_t,int> expected;
    for(uint32_t n=0;n<514;++n) {
        const auto key=n*73%257;const auto a=actual.emplace(key,int(n));const auto b=expected.emplace(key,int(n));
        check(a.second==b.second&&*a.first==*b.first&&actual==expected,"sorted insertion/duplicate outcome differs from std::map");
    }
    const auto& constant=actual;
    for(uint32_t key=0;key<300;++key) {
        const auto a=constant.find(key);const auto b=expected.find(key);
        check((a==constant.end())==(b==expected.end())&&constant.contains(key)==expected.contains(key),"find/contains differs from std::map");
        if(b!=expected.end())check(constant.at(key)==b->second,"const at differs from std::map");
        const auto lower=constant.lower_bound(key),end=constant.end();const auto oracle=expected.lower_bound(key);
        check((lower==end)==(oracle==expected.end())&&(lower==end||lower->first==oracle->first),"lower_bound differs from std::map");
    }
    const auto before=actual;rejects([&]{actual.at(999);},"missing mutable at did not reject");rejects([&]{constant.at(999);},"missing const at did not reject");check(actual==before,"missing at inserted a default row");
    for(uint32_t key=0;key<257;key+=3)check(actual.erase(key)==expected.erase(key),"erase differs from std::map");
    actual[999]=47;expected[999]=47;check(actual==expected&&expected==actual,"default insertion/legacy comparison differs from std::map");
    check(std::equal(actual.crbegin(),actual.crend(),expected.crbegin()),"reverse order differs from std::map");
    SortedMap<uint32_t,int> copied;copied=actual;copied.at(999)=12;check(actual.at(999)==47,"copy assignment aliased the original dictionary");
    SortedMap<uint32_t,int> imported(expected);check(imported==expected,"legacy std::map import changed rows");imported.clear();imported=expected;check(imported==actual,"legacy std::map assignment changed rows");
}
struct CopyFailure {};
struct Fragile {
    int value=0;static inline bool fail=false;
    explicit Fragile(int v=0):value(v){}
    Fragile(const Fragile& other):value(other.value){if(fail)throw CopyFailure{};}
    Fragile(Fragile&&)=default;
    Fragile& operator=(const Fragile&)=default;
    Fragile& operator=(Fragile&&)=default;
    bool operator==(const Fragile&)const=default;
};
void allocation_failure() {
    SortedMap<uint32_t,Fragile> value;value.emplace(2,20);value.emplace(4,40);const auto before=value;
    Fragile::fail=true;rejects([&]{value.emplace(3,30);},"failed staged insertion did not throw");Fragile::fail=false;check(value==before,"failed insertion changed the existing dictionary");
    SortedMap<uint32_t,Fragile> destination;destination.emplace(9,90);const auto kept=destination;
    Fragile::fail=true;rejects([&]{destination=value;},"failed staged copy did not throw");Fragile::fail=false;check(destination==kept,"failed copy assignment partially published rows");
}
void optional_publication() {
    std::optional<NationalCampaignHistory> national=NationalCampaignHistory{};NationalCampaign season;season.season=0;national->seasons.emplace(0,season);national->honours.push_back({});
    std::optional<AnnualTitleArchive> archive=AnnualTitleArchive{};archive->unresolved_source_labels[0][0]="Authored title";archive->seasons.push_back({});
    std::optional<RegionalPlayoffSeason> regional=RegionalPlayoffSeason{};regional->activation_deductions.emplace(ClubId{11},3);
    std::optional<FriendlyPlan> friendly=FriendlyPlan{};friendly->national_progress.emplace(ClubId{17},1);friendly->pending.push_back({});
    decltype(national) next_national;decltype(archive) next_archive;decltype(regional) next_regional;decltype(friendly) next_friendly;
    reject_allocations=true;
    next_national=std::move(national);next_archive=std::move(archive);next_regional=std::move(regional);next_friendly=std::move(friendly);
    reject_allocations=false;
    check(next_national->seasons.size()==1&&next_archive->unresolved_source_labels.at(0)[0]=="Authored title"&&next_regional->activation_deductions.at({11})==3&&next_friendly->national_progress.at({17})==1,"allocation-free optional publication lost its actual rows");
}
void codecs() {
    TitleHistoryArchive legacy;legacy[31][0]="Authored later title";legacy[0][1]="Authored earlier title";legacy[0][1].push_back(char(0));legacy[0][1].push_back(char(0x9b));
    AnnualTitleArchive archive;archive.earlier_history_unknown=true;archive.unresolved_source_labels=legacy;
    auto encoded=encode_annual_title_archive(archive);const auto canonical=cli::dump_json(encoded);
    auto& labels=std::get<cli::Json::Array>(encoded.object()->at("unresolved_source_labels").value);std::reverse(labels.begin(),labels.end());
    const auto decoded=decode_annual_title_archive(encoded);check(decoded&&decoded->unresolved_source_labels==legacy&&cli::dump_json(encode_annual_title_archive(decoded))==canonical,"actual title codec changed sorted wire order or unresolved Source bytes");
    labels.push_back(labels.front());rejects([&]{decode_annual_title_archive(encoded);},"actual title codec accepted duplicate dictionary keys");
    RegionalPlayoffSeason regional;regional.dates={1,2,3,4,5,6};regional.activation_day=0;regional.activation_deductions={{ClubId{13},1},{ClubId{11},3}};
    auto regional_json=encode_regional_playoff(regional);const auto regional_wire=cli::dump_json(regional_json);auto& cuts=std::get<cli::Json::Array>(regional_json.object()->at("activation_deductions").value);std::reverse(cuts.begin(),cuts.end());
    const auto restored=decode_regional_playoff(regional_json);check(restored&&*restored==regional&&cli::dump_json(encode_regional_playoff(restored))==regional_wire,"actual playoff codec changed sorted dictionary order or payload");
    cuts.push_back(cuts.front());rejects([&]{decode_regional_playoff(regional_json);},"actual playoff codec accepted duplicate dictionary keys");
}
}
int main()try {
    map_oracle();allocation_failure();optional_publication();codecs();
    std::cout<<"Sorted saved dictionaries: std::map oracle, immutable keys, strong staging, allocation-free optional moves and real codec order/bytes passed\n";
    return 0;
}catch(const std::exception& error){reject_allocations=false;std::cerr<<error.what()<<'\n';return 1;}
