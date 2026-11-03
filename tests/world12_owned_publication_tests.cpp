// SPDX-License-Identifier: GPL-3.0-only
#include "app/owned_file_publication.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace bmh;using namespace bmh::app::world12;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid file publication was accepted");}
Bytes bytes(const char* text){const std::string value(text);return Bytes(value.begin(),value.end());}
void write(const std::filesystem::path& path,const Bytes& value){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(value.data()),value.size());check(bool(out),"fixture file write failed");}
struct Directory {std::filesystem::path path;~Directory(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}};
}
int main()try{
    Directory directory{std::filesystem::temp_directory_path()/("openbmh-owned-publication-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};std::filesystem::create_directory(directory.path);
    const auto score=directory.path/"score",automatic=directory.path/"automatic";
    write(score,bytes("before"));
    publish_owned_files({{score,bytes("before"),bytes("after")},{automatic,{},bytes("save")}});
    check(read_file(score)==bytes("after")&&read_file(automatic)==bytes("save"),"owned score and automatic save did not publish");
    rejects([&]{publish_owned_files({{score,bytes("before"),bytes("wrong")}});});check(read_file(score)==bytes("after"),"stale publication replaced an external change");
    const auto created=directory.path/"new-parent-file";
    rejects([&]{publish_owned_files({{score,bytes("after"),bytes("uncommitted-score")},{created,{},bytes("temporary")},{created/"child",{},bytes("cannot-create")}});});
    check(read_file(score)==bytes("after")&&!std::filesystem::exists(created),"later I/O failure failed to restore an earlier replacement and remove its new file");
    rejects([&]{publish_owned_files({{score,bytes("after"),bytes("first")},{score,bytes("after"),bytes("second")}});});
    check(read_file(score)==bytes("after"),"duplicate publication target changed bytes");
    publish_owned_files({{directory.path/"empty",{},Bytes{}}});check(std::filesystem::file_size(directory.path/"empty")==0,"known empty publication was confused with an unknown file");
    std::cout<<"Owned file publication: score/save commit, stale/duplicate rejection, complete rollback after a later I/O failure passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
