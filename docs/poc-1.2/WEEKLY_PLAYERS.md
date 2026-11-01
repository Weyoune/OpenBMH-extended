# Weekly players and human training

The daily coordinator must retain the Original order: weekly manager incidents,
market countdown and bids, player injuries/recovery, manager settlement/statistics,
then familiarity and human training. The new helpers implement the player and
training phases of that sequence. They do not advance a calendar or complete the
still-unimplemented daily transaction.

`weekly_player_update` is the shared Core operation for one **physical roster
slot**. Its full legacy caller keeps the original slot order and the `0x870`
foreign-squad boundary. Recovery can reorder an assistant-controlled lineup;
subsequent slots observe that changed roster, exactly as in the old loop.
`weekly_processing` uses the extracted full pass without changing the surrounding
manager, market or settlement processing.

`world12::update_weekly_players` traverses Germany, England and the remaining
international leagues in their saved membership order. It processes at most 32
complete club rosters per numeric workspace and maps every change back through
checked global club/player IDs. This supports more than 4,095 players without
packing or aliasing their identities. A playable domestic league receives the
Original domestic injury policy; an international AI league receives the foreign
throttle. England's AI-only fallback is therefore distinct from its playable
ladder. Native global team numbers do not determine country policy.

The phase retains training injuries, protection flags, physician-adjusted absence,
recovery, permanent career endings and the Original assistant lineup updates.
It returns all generated notices in order, preserving their 150 raw text bytes,
date and globally qualified manager recipient mask. A small fresh mailbox captures
each player's insurance/injury notices, so workspace batches cannot drop earlier
messages. The application must retain these notices in its persistent mailbox;
this returned list does not itself implement the mailbox UI or its storage policy.

`world12::train_manager_squads` runs the shared familiarity decay once, then trains
the human managers in saved order with their actual coaching and tactical state.
Training now reads the projected club's round cursor. Two countries can share a
division class while having different progress; neither a temporary team number
nor one legacy league array can represent that distinction. Ordinary legacy
states keep their original round lookup.

Both helpers stage the career and its two random streams, check for external
changes before publication and preserve existing snapshot fields. They add no
world JSON or snapshot schema. The application still needs to combine them with
the missing manager/economy/market phases, persistent news and daily ownership
before enabling progression after the final desk. National coaching needs its
separate handback/takeover integration.

Validation covers a synthetic 4,680-player world, IDs above signed 32-bit/native
packed ranges, sparse manager seats, physical-slot order, foreign throttling,
recovery/retirement, unrelated-player preservation, different country rounds,
poisoned legacy round arrays, snapshots and late-callback rollback. Private checks
compare ten actual-data player/training phases with the complete Core, including
80 injuries, ten recoveries and a retirement, full boards/rosters/notices and both
RNG streams. A separately retained pre-change Core library produces byteidentical
complete legacy save streams and RNG traces for 24 controlled weekly/training
cases across modes, difficulty levels and seeds. This is an unchanged-Core
regression check, not a new DOS recording or a playable 1.2 release claim.
