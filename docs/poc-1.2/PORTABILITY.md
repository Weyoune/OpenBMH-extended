# Development portability checks

The first shared-world Windows CI build exposed a throwing move-construction
path in `optional<InitialCompetitionFields>`: MSVC's tree containers allocate a
sentinel when moved into a previously empty optional. That made publication of a
staged `CareerSnapshot` potentially throwing. The existing no-throw assertions
correctly stopped the build; they remain in place.

The two small country/title lookup maps now use sorted country/club rows. Their
move construction does not allocate. Decoding still rejects duplicate countries
and canonicalizes their order; world validation also rejects duplicates or
noncanonical row order. The encoded JSON shape, country identities and snapshot
schema are unchanged. Tests assert both no-throw construction of the field set
and no-throw assignment of the complete snapshot.

macOS CI also exposed a test assumption about temporary-directory spelling.
`checked_game_path` correctly returns the canonical confined target, while the
test compared it with an uncanonicalized `/var` path. The test now compares the
canonical parent and also exercises an explicit directory alias when supported.
Symlink *file* targets remain rejected. This changes the test, not save-path
confinement.

Native CI build/test results are separate from manual graphical platform QA.
Neither this change nor a successful Linux build establishes release readiness.

The next Windows build passed the snapshot assertions and found the same issue
in desktop publication: an optional `LeImage` can construct a tree-backed runtime
policy while accepting a prepared view. Prepared views and the retained desktop
reference now own their image through `unique_ptr`. Image copying/allocation
happens before publication. The complete prepared-view move remains checked as
no-throw, without weakening the existing transactional guarantee.
