# National-only inactive club setup

Schema42 `inactive_club_state` retains numerical club setup fields separately
from actual NationalCampaigns. It has no played club games, scores, completed
standings, active cup owners, finances or hidden human club assignment.

The startup factory requires the actual master database as well as the retained
national-only population. It captures the 27 real seven-club reserve pools whose
entities are intentionally scoped out of national-only gameplay. It retains the
Source physical holder/primary-cup setup seeds and the original primary cup
runner word. These seeds are explicitly unplayed setup operands, not trophies.

Initial national selection, foreign cup sampling, European fill/shuffles and
primary-country domestic field shuffles use shared Core kernels. The already
drawn national four field passes to the national campaign initializer without
another draw. Later actual national draws precede each inactive club field
reset. The common bounded CW allocation correction also applies when an
unplayed foreign winner and finalist both occupy CL slots; it uses the affected
country's highest available physical slot without claiming a played ranking.

National-only Extended chooses a nation directly. It intentionally has no
Original hidden club/person selection. Actual nominated squads train once under
the national service policy and own their separate actual national account.
There is no hidden club advertising, finance, tickets, training or ordinary
weekly/monthly business. Whole-Original national-career random-stream parity is
not claimed. Core match, nomination, ageing and retirement rules, explicit saved
facts and deterministic staged publication remain required.

The public test uses eight controlled-score actual Core national programmes for
each primary nation, with repeated setup/save/stale/replay checks and fixed Core
shuffle counts. These are not played calendar seasons or release evidence.
