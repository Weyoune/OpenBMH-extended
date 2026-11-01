# Authoritative weather and referee resources

The preparation primitive exposes the same Core month/region temperature table,
two weather draws, unsigned-byte snow rule and referee selection as native
matchday setup. Native preparation delegates to this primitive without changing
valid-match draw order. An exhausted/short pool rejects before drawing instead
of entering the Original unbounded retry loop.

The shared world can schedule 117 fixtures on one date. Referee availability is
therefore country-scoped: each home country's programme has its own mask over
the supplied 50-entry catalogue. German and English full rounds each need at
most 45 of the 49 selectable entries; Spain/France/Italy need nine. Duplicate
selection inside a country is forbidden. The source properties and names are
shared resources; separate real-world national referee databases are not claimed.

`prepare_fixture_weather` preflights identities and pool sizes before consuming
randomness, preserves the global fixture identities and publishes both random
streams only after complete preparation. The career event owner must store its
result with the prepared event before displaying or saving a pre-match desk.
That event/UI integration is not implemented by this primitive.

## Data-only resource

Current standalone exports include `REFEREES.JSON` in COMMUNITY.OBD. It contains:

- `format`: `openbmh-referees`;
- `version`: 1;
- `names`: exactly fifty nonempty source-encoding strings;
- `properties`: exactly fifty source byte values.

The exporter reads the original catalogue from the supplied base OBD. No master
M3D database or original executable is needed by the runtime. Runtime loading
rejects missing, malformed or unsupported catalogues with an explicit rebuild
instruction. Older development packages need re-export; their player/world IDs
are unchanged. The package remains `playable:false` until all release gates pass.

Tests cover all 117 fixtures, country uniqueness, source skills, repeatable
random streams, late invalid requests, invalid resource versions/dimensions,
zero/one-degree snow versus negative temperature bytes and pool exhaustion.
The actual data-only session regression also passes with the private new export.
