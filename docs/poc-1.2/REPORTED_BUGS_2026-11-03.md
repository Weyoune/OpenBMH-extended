# Extended follow-up reports, 2026-11-03

Tracked as issues #36–43. This work is separate from the completed three-feature
milestone. No follow-up fix is claimed released until its own validation.

| Issue | Scope | Current evidence |
| --- | --- | --- |
| 36 | Current English data | Reuse private Dreierpack/FIFA sources; updated package and provenance pending |
| 37 | English camps | Current renderer always loads German KB/12.VGA; English assets, coordinates and rules need country-specific binding |
| 38 | Club information | Typed adapter explicitly disables players/statistics/category; needs bounded modal services and persisted presentation ownership |
| 39 | Europe selector | Replacement prepared using the original selector atlas instead of setup globe |
| 40 | Pre-match screens | Typed startup deliberately leaves MatchdayIntro unconstructed; needs actual fixture projection and country gap |
| 41 | Trophy win screens | User suspects missing scenes; reproduce real finals before deciding missing presentation paths |
| 42 | Irrelevant national draw | Initial callback was unconditional and used outer session identity; prepared fix filters human national viewers and passes actual staged identity |
| 43 | Second music track | Typed startup omitted the shared career music command; prepared once-per-successful-start/load switch respecting saved options |

First code batch builds successfully. Eight focused session/national regression
tests pass. Native UI/audio confirmation, before/after evidence and remaining
items are still pending. Original data and extracted FLTD assets belong only in
private data/evidence repositories. The 32-bit money limit remains out of scope.
