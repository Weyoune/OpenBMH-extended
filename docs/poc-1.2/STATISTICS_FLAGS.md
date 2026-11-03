# Owned statistics display flags

Schema41's reserved `ManagerCareerState.statistics_flags` JSON slot now contains
independent optional `statistics` and `player_statistics` bytes. They represent
Source Flags offsets0 and2 respectively. A null/missing slot retains both as
unknown; each member may independently remain null. The existing
`stadium_curve_flags` owner remains the sole owner of byte1.

Shared manager projection installs only known bytes and preserves neighboring
flags and canonical seat mappings. Ordinary capture preserves older unknowns.
An owned Source action that actually wrote a flag may call
`capture_manager_statistics_flags` with mask1 for statistics byte0, mask4 for
player-statistics byte2, or mask5 for both. This explicit capture promotes only
the requested actual memory values; it does not infer a default or touch the
curve byte. Annual reset may capture its actual newly written bytes the same way.

These are display preferences. The adapter and schema changes do not redraw a
player card, consume random streams, alter player statistics, or reconstruct
missing game history. The existing Source card/controller owns any genuine
redraw/RNG boundary. Old schema40 cannot restore known41 preference values.
