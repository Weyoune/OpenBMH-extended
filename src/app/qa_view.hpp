// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
namespace bmh::app {
// Explicit host-test instrumentation. It observes presentation tags only and
// has no GameState, input, calendar or RNG authority. Empty path disables I/O.
class QaViewTrace {
public:
    explicit QaViewTrace(std::filesystem::path path={}):path_(std::move(path)){}
    void begin_scope(){last_.clear();identity_.clear();}
    void note(std::string_view view,std::string_view identity={}){
        if(path_.empty()||(view==last_&&identity==identity_))return;
        last_=view;identity_=identity;++sequence_;
        std::ofstream output(path_,std::ios::trunc);
        if(output)output<<sequence_<<'\t'<<view<<'\n';
    }
private:
    std::filesystem::path path_;
    std::string last_,identity_;
    uint64_t sequence_=0;
};
}
