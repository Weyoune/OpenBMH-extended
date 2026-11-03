# National known facts and inactive club state

Schema42 owns the national-only startup receipt, view preferences and desktop
layout introduced during development, and the optional Source national-account
words `investment_balance` (+0x324) and `depreciation` (+0x30c). These financial
words are independent from the ordinary club account. Missing old words remain
unknown; zero is a known value only after actual capture or an explicit owned
write. Each national-coach row stores these nullable words separately.

A forged older schema containing any nonnull42 fact is rejected. Null or absent
old facts remain unknown. Statistics flag byte preferences retain their existing
schema41 payload and do not move into42.

The top-level `inactive_club_state` key is reserved as null. Nonnull payloads are
rejected until the actual unsimulated club setup/market owner is implemented.
National-only play must preserve Source numerical draw and population obligations
without publishing club games, completed standings or club trophy facts. The
reserved key is not evidence that this remaining annual path is complete.
