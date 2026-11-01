// SPDX-License-Identifier: GPL-3.0-only
#include "launch/fltd.hpp"
#include "formats/le.hpp"
#include "game/calendar.hpp"
#include <iostream>
int main(int argc,char** argv)try {
    if(argc!=3)throw std::runtime_error("usage: world12_export_english_runtime FLTD.EXE RULES-EN.DAT");
    const auto raw=bmh::read_file(argv[1]);const auto epoch=bmh::calendar_epoch_from_fltd(raw);
    const auto image=bmh::launch::translate_fltd(argv[1]);
    bmh::save_runtime_image(image,{uint32_t(epoch.year),uint32_t(epoch.weekday),90,0,{}},argv[2]);
    std::cout<<"English data-only rules exported; no original executable required at runtime\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
