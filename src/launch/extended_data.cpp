// SPDX-License-Identifier: GPL-3.0-only
#include "launch/extended_data.hpp"
#include "formats/resource_bundle.hpp"
#include "cli/json.hpp"
#include <algorithm>
#include <cctype>

namespace bmh::launch {
namespace {
std::string value(const cli::Json& manifest,const char* key) {
    const auto* object=manifest.object();
    if(!object)return {};
    const auto found=object->find(key);
    return found!=object->end()&&found->second.string()?*found->second.string():std::string{};
}
}
ExtendedDataCheck inspect_extended_data(const std::filesystem::path& input,bool allow_development) {
    ExtendedDataCheck out;
    try {
        std::error_code ec;
        if(std::filesystem::is_directory(input,ec))out.directory=input;
        else {
            auto extension=input.extension().string();
            std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(extension!=".obd"&&extension!=".exe")return out;
            out.directory=input.parent_path();
            if(out.directory.empty())out.directory=".";
        }
        const bool world=resource_exists(out.directory/"WORLD12.JSON");
        out.recognized=world;
        if(!resource_exists(out.directory/"PACKAGE.json")) {
            if(world)out.error="Extended data requires PACKAGE.json metadata";
            return out;
        }
        const auto raw=read_file(out.directory/"PACKAGE.json");
        if(raw.size()>1024*1024)throw FormatError("Extended package metadata exceeds its size limit");
        const auto manifest=cli::parse_json(std::string(raw.begin(),raw.end()));
        out.recognized=world||value(manifest,"format")=="openbmh-community-12"||
            value(manifest,"profile")=="dual-classic-2"||value(manifest,"requires")=="runtime:dual-classic-2";
        if(!out.recognized)return out;
        if(!manifest.object()||value(manifest,"format")!="openbmh-community-12"||
           value(manifest,"profile")!="dual-classic-2"||value(manifest,"requires")!="runtime:dual-classic-2")
            throw FormatError("Extended package requires openbmh-community-12, dual-classic-2 and runtime:dual-classic-2 metadata");
        const auto published=manifest.object()->find("playable");
        if(published==manifest.object()->end()||!published->second.boolean())
            throw FormatError("Extended package playable metadata must be a boolean");
        out.playable=*published->second.boolean();
        for(const auto* required:{"WORLD12.JSON","RULES-EN.DAT","KB/SETUP12.VGA"})
            if(!resource_exists(out.directory/required))throw FormatError(std::string("Extended data is missing ")+required);
        if(!out.playable&&!allow_development)
            throw FormatError("This Extended data package is unpublished; use --extended-dev only for explicit development testing");
    }catch(const std::exception& error) {
        // A damaged bundle or recognized manifest cannot be safely reclassified
        // as another edition merely because its metadata failed to load.
        out.recognized=true;out.error=error.what();
    }
    return out;
}
}
