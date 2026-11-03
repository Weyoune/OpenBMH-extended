# National known facts and inactive club state

Schema42 owns the national-only startup receipt, view preferences and desktop
layout introduced during development, and the optional Source national-account
words `investment_balance` (+0x324) and `depreciation` (+0x30c). These financial
words belong to the national account. The +0x324 word already has a canonical
owner in `finance.baseline.starting_balance`; its nullable mirror must agree
when known and never substitutes a second balance. Missing old words remain
unknown; zero is a known value only after actual capture or an explicit owned
write. Each national-coach row stores these nullable words separately.

A forged older schema containing any nonnull42 fact is rejected. Null or absent
old facts remain unknown. Statistics flag byte preferences retain their existing
schema41 payload and do not move into42.

The top-level `inactive_club_state` key owns actual unsimulated master setup
fields and the captured reserve pool registry. Null preserves explicit old
unknown state. This numerical setup creates no club games, completed standings
or club trophy facts. Its presence is not evidence of a played season or that
the complete national-only annual publication path is ready.
