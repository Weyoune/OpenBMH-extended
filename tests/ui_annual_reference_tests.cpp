// SPDX-License-Identifier: GPL-3.0-only
#include "ui/season_promotion.hpp"
#include "ui/game_end.hpp"
#include "ui/game_end_alternate.hpp"
#include "ui/setup.hpp"
#include <iostream>
using namespace bmh;using namespace bmh::ui;
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("usage: ui_annual_reference_tests <Original data>");
    if(!*argv[1])return 77;const std::filesystem::path data=argv[1];const auto reference=load_le(asset_path(data,"BMH.exe"));
    for(int group=0;group<4;++group){SeasonPromotionProjection projection;projection.group=group;projection.count=1;projection.league_footer="Authored league";projection.relegated={{0,0,false,"Authored down"}};projection.promoted={{1,1,false,"Authored up"}};
        const SeasonPromotionScreen native(data,projection),explicit_reference(data,projection,reference);
        auto actual_crests=projection;actual_crests.relegated[0].club_id=5000;actual_crests.promoted[0].club_id=5001;actual_crests.relegated[0].badge_fallback=actual_crests.promoted[0].badge_fallback=true;actual_crests.relegated[0].badge_atlas=actual_crests.promoted[0].badge_atlas="W/0.VGA";
        const SeasonPromotionScreen canonical(data,actual_crests,reference);
        for(int step:{0,1,19,38}){const auto expected=native.render(step);check(expected.pixels==explicit_reference.render(step).pixels,"promotion explicit reference changed Native pixels/scanline progression");check(expected.pixels==canonical.render(step).pixels,"explicit promotion crest leaked drawing ID into Native fallback or changed actual cell/path");}
    }
    HighscoreProjection scores;scores.signature=135;scores.ranked=true;scores.rows[0].score=12345;scores.rows[0].club_name="Authored club";scores.rows[0].manager_name="Authored manager";scores.rows[0].titles={1,2,3};
    for(bool plain:{false,true})for(bool gradient:{false,true}){const GameEndRenderOptions options{plain,gradient};check(GameEndHighscoreScreen(data,scores,options).render().pixels==GameEndHighscoreScreen(data,scores,options,reference).render().pixels,"highscore explicit reference changed Native formatting/fonts/pixels");}
    const auto native_assets=load_game_end_alternate_score_assets(data),explicit_assets=load_game_end_alternate_score_assets(data,reference);const Image backdrop(320,240,7);GameEndAlternateScoreScreen native(native_assets,backdrop),explicit_reference(explicit_assets,backdrop);check(native.render().pixels==explicit_reference.render().pixels,"alternate calculator explicit reference changed Native pixels");native.text("300");explicit_reference.text("300");check(native.render().pixels==explicit_reference.render().pixels&&native.value()==explicit_reference.value(),"alternate calculator explicit reference changed actual control outcome");
    std::cout<<"Original annual promotion/highscore/alternate explicit-reference constructors retain complete Native pixels; canonical crest path/cell wins over drawing-ID fallback\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
