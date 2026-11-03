# Pure saved competition views

`ui::world12_cup_draw_notice` projects an already captured real domestic
`CupDrawNotice` to the existing Original `CupDrawProjection`. It validates the
saved stage, round and exact ordered fixtures, retains Source headers/eligibility/
speed options, and exposes drawing tokens separately from canonical club IDs.
Every club carries its actual crest atlas and cell. It does not rerun a draw,
change a programme or consume either random stream. English main/League Cup
labels retain their actual country identity rather than using the German label.

European overview pages now read the actual saved campaigns, all knockout stages,
return-leg arrays, parallel group stages and completed finals. A genuinely
undrawn future stage remains unavailable; an older unknown programme is labelled
unknown. Native source score words, physical pair orientation, Source current
round and16-participant detail windows remain explicit. Existing Original
`OtherGamesScreen` renders the result; there is no independent cup renderer.

`world12_european_games_state` can accept actual current-score overlays for the
selected competition. Each overlay must match the saved campaign revision,
entry, played cursor and full actual current fixtures. Only the local display
copy receives those scores. Source group tables use the shared const
`project_source_group_table` adapter over `cup_group_table`; no round/qualification
or RNG is advanced. The returned renderer-token map retains canonical club IDs.
Root live presenters supply real current scores; these APIs never generate them.

Synthetic tests exercise52 saved European group/leg boundaries across all three
Source programmes, exact current fixtures, actual-score display overlays,
stale/altered fixture rejection, immutable state/seeds and actual captured draw
order/crest paths. Their authored test scores are not a played-season claim.
