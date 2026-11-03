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

Domestic live overlays authenticate the saved stage revision, Source entry,
complete ordered `next_fixtures()` and the pending leg cursor
`(rounds.size()-1) * legs + next_leg()-1`. They replace only local display
values. A half-time 0:0 is valid presentation data and never completes a
knockout round, chooses a qualifier or draws its successor. Return-leg words
retain their actual match orientation while the display aligns them with the
stored first-leg fixture. German and English Super-Cup finals remain separate
owners even when both use Source entry23 and revision0. Unknown, repeated,
stale, incomplete or wrongly oriented overlay receipts reject atomically.

The generic overview builder and navigation accept an optional explicit Source
`TextDb` after the overlay argument. Page4 uses the actual current national
campaign and its saved stage/round selection. Its linked-stage controls and
round arrows retain that owner; no national competition number is reinterpreted
as a domestic LeagueId. A missing text/programme context is explicit, rather
than a placeholder national fixture page.
