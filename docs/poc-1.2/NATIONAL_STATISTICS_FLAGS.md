# National presentation flags

Unreleased schema42 extends the existing optional national `view_preferences`
payload with `statistics_flags`: null, or an object with independent nullable
`statistics` and `player_statistics` byte values. These own Original Flags0/2
for NoClub careers. An older42 payload missing this member retains both
unknown; pre42 nonnull national preferences remain rejected by the existing
schema guard. Known zero is distinct from unknown. No manager career, club
history, portfolio or facility record is created.

Fresh national startup captures the actual supplied Source bytes. National
workspaces install known values and capture only previously known values, so a
reference default cannot reconstruct an older unknown preference. Full club
careers retain their established canonical person's presentation owner.

An explicit player-card checkbox write can promote byte2 independently through
`ClubModalServices::retain_player_statistics_flags`. The live scope stages this
receipt until successful finish; abort restores the Source state and publishes
no retained metadata. The shared projection then carries the canonical seat's
preference through its existing atomic match publication. Retired capabilities
reject. Main invokes the optional callback immediately after its actual Source
Flags2 write; existing legacy services may leave it empty.

Checks cover DE/EN startup capture, known/unknown codec preservation, malformed
bytes, actual projection install/capture, independent authored writes, complete
save roundtrips, live cancellation and retired callbacks. Neither flag changes
match rules or either random stream.
