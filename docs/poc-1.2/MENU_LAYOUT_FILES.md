# Original MEN layout files

The typed desktop's final quickbar cell now opens the Original layout file
chooser: left click saves, right click loads. Files remain exact 864-byte MEN
payloads containing the active manager's 54 layout records. They are distinct
from FBM careers and do not contain clubs, players, finance or random state.

Native and typed sessions share the payload validator. It checks the exact size,
bounded forward chain, valid terminators and cycles. The Original empty chain
and valid custom backlinks remain supported; no normalisation replaces unknown
source bytes. A typed load additionally validates the resulting current desktop
projection and builds the complete replacement renderer before publishing it.

The pending visible chooser, source record 53, manager, revision and file kind
must match. Files are confined to the configured save directory and must be
regular files with the MEN extension. Symlink destinations, foreign origins,
wrong extensions and unavailable overwrite confirmation are rejected. Oversized
or truncated layouts are rejected before reading an arbitrary-size payload.

Saving shares the game's checked-byte publisher: create an exclusive temporary,
write and flush it, read back the exact bytes, validate its payload, then publish
without truncating the previous destination. Failure preserves the old file and
removes the temporary. GUI validation happens before publication; the prepared
session and renderer are swapped only after success. Loads and saves change
neither date nor RNG. Layout changes stay within the current canonical seat.

Synthetic tests cover exact bytes, malformed/empty/custom-link payloads, path
confinement, symlinks where supported, overwrite refusal, directories and failed
publication. The shared FBM publisher is also checked after refactoring. Private
input tests save a layout, move an icon, restore the layout, confirm overwrite,
reject invalid records/renderer data, recover an empty desktop and preserve the
accepted career on all failures. Screenshots, MEN examples and matching FBMs
remain private.

There is no new gameplay/save schema. General settings files/windows, remaining
management views and the normal visible turn/release gates remain separate work.
