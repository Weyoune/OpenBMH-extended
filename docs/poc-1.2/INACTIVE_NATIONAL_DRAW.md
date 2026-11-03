# Disabled national Source draw carry

Native European setup draws the national field even when international play is
disabled. Schema41 retains this unsimulated scratch separately from real national
campaigns. It stores the current field, Source layout bases and untouched
entry13/14 qualifier words, normalized modulo16bits by the native national base.
Those words can refer to an unplayed club/scratch value. They are not global club
IDs, national winners, results, honours or match admission.

Fresh startup captures the four-field draw already performed by the shared
initial-field owner, without another RNG draw. Entry14 uses the existing Core
cold-seed initialization. CareerSeasonSetup's entry13 words are exactly
first-small-club+7 and national-base+1; disabled play never substitutes invented
national results for them.

`advance_inactive_national_draw(World&, target_season, G, W)` requires contiguous
saved ownership and calls the existing identity-neutral Source selection kernel
once, after club moods and before European qualification. The owner validates
and publishes the field and both streams atomically. It does not simulate a
national competition. Missing old40 history remains unknown and is rejected at
this annual boundary; nonnull41 carry cannot be silently downgraded.

The targeted regression checks twelve raw-token cycles, opaque/cold seeds,
shuffle counts, exact stream state, structural save roundtrips, no gameplay
mutation, stale calls and unknown-history/version boundaries. This is Source
scratch/RNG validation, not played-season proof.
