# Individual fixture rescheduling

**Verlegen** uses the Original calendar screen. Choose a future home league game,
then a highlighted replacement date. Clicking an eligible moved date restores the
original appointment. The first and final two rounds cannot be moved. The Source
winter break and replay-date window remain in force; floodlights permit the day
before the original round. There are forty pending slots and at most eight
replays per day, matching the shared Source rescheduling kernel.

The saved fixture identity is `(league, round, original match index)`. A move does
not alter memberships, the season's fixture matrix, other matches, any score or
either random stream. Both clubs' actual programmes and pending friendlies are
checked; all relevant domestic/shared cup and national dates are protected.
OpenBMH deviates from the original: collisions use full canonical club identities
instead of comparing low bytes from another league's positional team list.

On the original date, the other fixtures play normally. On the new date, the moved
fixture runs through the existing Source replay competition (type 9 / descriptor
10), including weather, attendance, Friday modifiers, actual human interventions,
post-match accounting and player records. Its result goes into the original
league cell exactly once. A Friday result appears in tables and live overviews
immediately; it does not advance the ordinary league-round cursor. Deferred
matches leave real unplayed cells until their replay finishes.

The league pairings' Nachholspiele view lists pending fixtures with dates. The
calendar marks replay dates, and friendly booking protects them. A pending move
can be undone only before both its original and replacement dates. Completed
receipts remain for the current season and are cleared by the actual annual
transition after season completion.

Typed save schema **OBMH12SN** adds the fixture ledger and replay group flag.
Older A–M saves remain readable with an empty ledger. Old readers reject N;
downgrading the marker cannot silently discard the extra fields. World schema
43 separately owns the balance plan. The existing data master needs no reimport.

Validation includes synthetic DE18/EN20 early and deferred dates, partial ordinary
rounds, exact undo, roundtrips, duplicate/actor/date/collision guards and the daily
capacity. Private supplied-data UI runs cover calendar selection/undo, save and
official resume, real early/late Source clocks, original result-cell mapping,
live table overlays and no second match on the ordinary date. Test-authored
scores occur only in synthetic transaction tests, not the private playthroughs.
