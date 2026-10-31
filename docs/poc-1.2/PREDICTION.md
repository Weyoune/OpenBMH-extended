# League predictions for typed human matches

`calculate_table_prediction` is the shared numeric Core kernel of the existing Void5 calculation. The legacy entry point still owns its unchanged early returns, stale league-size word, once-only goal flags and notification text/order. Numeric forecast data is no longer tied to physical team slots.

`world12::predict_league` supplies typed standings for any active league, including the ten-team English fifth division. It retains Source's two-point forecast tables, three-points-per-future-win maximum, low-word arithmetic, pessimistic equal-rate ties, recent-form windows and deduction treatment. The promotion/Europe threshold follows the country ladder; disabled England's Premier League has no domestic relegation. Club identities remain global. Early returns leave the optional source size word absent.

The typed service is read-only. Goal-notice bits are returned as data; the future career owner must publish its once-only notifications and flags. They are not emitted to a fake legacy manager or silently treated as a completed human match integration.

Verification on Linux:

- 600 typed/native forecasts with wide IDs, both ladder structures, deductions, mode windows, early/late rounds and notice masks.
- 50 ten-team boundary and tie cases.
- 15,120 private comparisons against the pre-refactor Core implementation, including complete rank buffers, untouched/stored size words, goal flags and mailbox bytes. Distinct synthetic text tokens check notice ordering as well as counts. This is a pre-change implementation comparison, not a new Original-DOS oracle claim.
- All 20 targeted regression tests and the application build pass.

This prepares the table/motivation/prize context needed by human match projections. Human scoring/live ownership and complete career/GUI integration remain pending.
