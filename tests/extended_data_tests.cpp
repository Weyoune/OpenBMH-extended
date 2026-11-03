// SPDX-License-Identifier: GPL-3.0-only
#include "launch/extended_data.hpp"
#include "launch/game_data.hpp"
#include "formats/binary.hpp"
#include "formats/resource_bundle.hpp"
#include "cli/json.hpp"
#include "miniz.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
using namespace bmh;
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void write(const fs::path& path,const Bytes& bytes){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));if(!out)throw std::runtime_error("synthetic package write failed");}
Bytes bytes(const std::string& text){return {text.begin(),text.end()};}
cli::Json manifest(bool playable=true){return cli::Json::Object{{"format","openbmh-community-12"},{"profile","dual-classic-2"},{"requires","runtime:dual-classic-2"},{"playable",playable}};}
std::map<std::string,Bytes> resources(const cli::Json& metadata){return {{"PACKAGE.json",bytes(cli::dump_json(metadata))},{"WORLD12.JSON",bytes("{\"authored_test\":true}")},{"RULES-EN.DAT",bytes("authored rules placeholder")},{"KB/SETUP12.VGA",bytes("authored setup placeholder")}};}
Bytes zip(const std::map<std::string,Bytes>& entries){mz_zip_archive archive{};check(mz_zip_writer_init_heap(&archive,0,0),"synthetic ZIP init failed");for(const auto& [name,data]:entries)check(mz_zip_writer_add_mem(&archive,name.c_str(),data.data(),data.size(),MZ_BEST_COMPRESSION),"synthetic ZIP entry failed");void* raw=nullptr;size_t size=0;check(mz_zip_writer_finalize_heap_archive(&archive,&raw,&size),"synthetic ZIP finalization failed");Bytes result(static_cast<uint8_t*>(raw),static_cast<uint8_t*>(raw)+size);mz_free(raw);mz_zip_writer_end(&archive);return result;}
void loose(const fs::path& directory,const std::map<std::string,Bytes>& entries){for(const auto& [name,data]:entries)write(directory/name,data);}
}
int main()try {
    const auto root=fs::temp_directory_path()/("openbmh-extended-launch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{root};
    const auto classic=root/"classic";write(classic/"BMH.EXE",bytes("authored classic placeholder"));
    check(!launch::inspect_extended_data(classic).recognized,"Classic data was admitted as Extended");
    write(classic/"PACKAGE.json",bytes(cli::dump_json(cli::Json::Object{{"format","openbmh-community-1"},{"profile","classic-plus-1"}})));
    check(!launch::inspect_extended_data(classic).recognized,"Classic package metadata was reclassified as Extended");
    const auto published=root/"published";loose(published,resources(manifest()));
    for(const auto& input:{published,published/"COMMUNITY.OBD",published/"BMH.EXE",published/"bmh.exe"}){
        const auto result=launch::inspect_extended_data(input);check(result.recognized&&result.playable&&result.error.empty()&&result.directory==published,"published directory/OBD/virtual EXE probe failed");
    }
    check(!launch::inspect_extended_data(published/"unrelated.zip").recognized,"ZIP path was mistaken for its adjacent installed directory");
    const auto draft=root/"draft";loose(draft,resources(manifest(false)));
    const auto normal=launch::inspect_extended_data(draft);const auto development=launch::inspect_extended_data(draft,true);
    check(normal.recognized&&!normal.playable&&!normal.error.empty()&&development.recognized&&!development.playable&&development.error.empty(),"unpublished data bypassed the explicit development gate");
    size_t invalid=0;
    for(const auto* field:{"format","profile","requires","playable"})for(bool missing:{false,true}){
        auto metadata=manifest();if(missing)metadata.object()->erase(field);else metadata.object()->at(field)=field==std::string("playable")?cli::Json(1):cli::Json("unsupported");
        const auto directory=root/("invalid-"+std::to_string(invalid++));loose(directory,resources(metadata));
        const auto result=launch::inspect_extended_data(directory,true);check(result.recognized&&!result.error.empty(),"invalid Extended metadata allowed Classic fallback or development bypass");
    }
    {const auto directory=root/"no-metadata";write(directory/"WORLD12.JSON",bytes("{}"));const auto result=launch::inspect_extended_data(directory,true);check(result.recognized&&!result.error.empty(),"missing Extended metadata allowed Classic fallback");}
    {const auto directory=root/"malformed";auto entries=resources(manifest());entries["PACKAGE.json"]=bytes("{");loose(directory,entries);const auto result=launch::inspect_extended_data(directory,true);check(result.recognized&&!result.error.empty(),"malformed Extended metadata allowed Classic fallback");}
    for(const auto* missing:{"WORLD12.JSON","RULES-EN.DAT","KB/SETUP12.VGA"}){
        auto entries=resources(manifest());entries.erase(missing);const auto directory=root/("missing-"+std::to_string(invalid++));loose(directory,entries);const auto result=launch::inspect_extended_data(directory,true);check(result.recognized&&!result.error.empty(),"incomplete published Extended resources were accepted");
    }
    const auto bundled=root/"bundled";const auto payload=zip(resources(manifest()));write(bundled/kCommunityBundle,payload);
    check(!fs::exists(bundled/"WORLD12.JSON"),"bundle fixture accidentally has loose data");
    for(const auto& input:{bundled,bundled/kCommunityBundle,bundled/"BMH.exe"}){const auto result=launch::inspect_extended_data(input);check(result.recognized&&result.playable&&result.error.empty(),"bundled Extended package was not recognized");}
    const auto bundled_draft=root/"bundled-draft";write(bundled_draft/kCommunityBundle,zip(resources(manifest(false))));
    check(!launch::inspect_extended_data(bundled_draft).error.empty()&&launch::inspect_extended_data(bundled_draft,true).error.empty(),"bundled unpublished data bypassed its development gate");
    const auto archive=root/"release.zip";write(archive,zip({{"release/COMMUNITY.OBD",payload}}));
    const auto resolved=launch::resolve_game(archive,root/"cache");
    check(resolved.unpacked&&!resolved.check.directory.empty(),"existing archive resolver did not retain the extracted bundle directory");
    const auto extracted=launch::inspect_extended_data(resolved.check.directory);check(extracted.recognized&&extracted.playable&&extracted.error.empty(),"extracted Extended bundle fell through to Classic startup");
    {const auto directory=root/"broken";write(directory/kCommunityBundle,bytes("damaged authored ZIP"));const auto result=launch::inspect_extended_data(directory);check(result.recognized&&!result.error.empty(),"damaged bundle silently fell back to another edition");}
    std::cout<<"Extended launch metadata, explicit development gate, directory/OBD/EXE and extracted bundled resources passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
