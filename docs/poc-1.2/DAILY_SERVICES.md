# Ordered daily services

`process_daily_services` composes finance and the Source `DayStepRest` sequence
for a date the caller has already advanced. It requires the exact previous date,
season, verified installed calendar epoch and an explicit event/plain-day policy.
Stale clocks fail before processing. The complete candidate, accounts and both
random streams are published together; a later weekly/monthly failure also rolls
back the earlier daily interest or stock update.

Plain-day entry performs its stock update and interest. Event entry performs
interest. The previous weekday then selects training; Thursday's weekly phase
also handles offers, auctions, bids, player updates and accounts before training.
A changed month follows training with the complete monthly transaction. Current
competition relationships and human decision presenters remain caller-owned.

The Source `next_event` loop performs an additional stock update after an event's
`DayStepRest`, before inspecting the calendar. This is separate from the above
services. `post_event_stock_pending` identifies that boundary;
`update_daily_stock_market` implements the tick without repeating interest or
training. If the new date is quiet, the following plain-day step has its own
stock tick. Conversely, the season-end desk returns before the extra tick.

The helper does not advance the date/calendar, prepare a match, seek the next
event, open a desk, hand a national coach back or perform annual awards. The
future saved progression owner must compose those actions atomically with these
services; it must not skip appointments. No GUI day-advance fallback is enabled.

Synthetic checks follow35 dated service steps across training, weekly and month
boundaries, retain snapshots, enforce the separate post-event tick, stop that
tick at season end and verify rollback after daily finance but stale weekly
facts. Seventy full-Core service comparisons advance dates through two separate
35-day fixtures, checking players, all rosters, full human boards/accounts/career
state, mail, stocks, aggregate draws and saved snapshots. Original club-play flags
are explicitly installed for this oracle. Fund holdings are zero in the dated
fixtures; the per-holder dividend correction has separate monthly comparisons.
Matches and calendar appointments are deliberately not played in these service
fixtures. They are not two played months or a qualified playable release.
