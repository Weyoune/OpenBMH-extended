# Typed graphical save and load

The development SDL entry now uses the Original file dialog for typed O1.2 snapshots. Save/load icons on the desktop and the setup load button route to the typed owner before any legacy file handler. Ordinary Classic directories retain their existing constructors and file path.

## Resume without career generation

`Session::resume` validates the saved world, complete manager state and its retained event/desktop cursor, derives options from saved settings/scope, and takes a fresh runtime owner revision. It does not call new-career initialization, generate people, redraw competitions, sell tickets, reset a season, enter the desk or consume randomness. Existing initial competition notices/ticket receipts are reconstructed read-only from their stored state. Local modal ownership is empty after load.

The current event model describes the first preparation day; snapshots without its required runtime owner are rejected explicitly. General subsequent-day ownership remains an implementation gate. Older S4–S7 containers can still be decoded by the format reader, but incomplete development snapshots are not turned into a new career to make them appear loadable.

## Original dialog, typed preview

`SaveLoadScreen` accepts an explicit rendering reference and a preview reader callback. Typed previews decode O1.2 directly and show its saved date, actual manager/club names and league names. No native FBM header, fixed nine-league ID, legacy database or guessed name-pool address is used for that preview. The original dialog geometry stays intact; each info column is bounded to its panel.

Load from setup works without an existing Session and honours saved settings even when the setup country switches differ. Loading from a desktop can replace its world with another valid saved scope/manager roster. Renderers are constructed before publishing the loaded Session, and the new UI revision exceeds the old dialog's revision. Failed decode/resource/render checks preserve the running career and leave the dialog usable.

## Save transaction

A matching actual dialog request and idle active-manager/revision are required. The target must be a regular `.FBM` in the configured save directory; symlinks, outside directories, wrong file kinds, directories as targets and unconfirmed overwrites are rejected.

Manual save retains the Original four Watcom draws for save ID/key/random byte. The staged snapshot stores the post-save seed; the game RNG and calendar remain unchanged. A successful manual write also fulfils a retained pending-save flag. The encrypted file keeps the typed snapshot framing (currently SA, with world schema 20 for economy, messages and retained market references); no fabricated legacy plain information header is added.

The controller stages both Session and rendered desktop first. The writer creates an exclusive temporary in the destination directory, writes/flushes/closes it, reopens/decodes it and compares the complete snapshot. Publication uses same-directory atomic replacement for a confirmed overwrite, or no-replace publication for a new file. Linux/macOS use rename or an exclusive hard-link publication; Windows uses CreateFileW/MoveFileExW. Failures before publication remove the temporary and preserve the previous target and live RNG/revision. Successful publication is followed only by nonthrowing Session/UI moves.

Automatic save scheduling/writing at future event barriers is still a day-owner gate; this milestone does not claim that scheduling has been integrated.

## Verification scope

Synthetic tests cover six saved country scopes, unchanged resume state and zero resume draws, exact manual-save header/four draws, stale save replay, unsupported missing event state, real container roundtrips, no-clobber collisions, directory/path/symlink rejection and temporary cleanup. Actual GUI input tests cover save, overwrite confirmation, bad target, preview bounds, desktop reload after a changed tactic, corrupt-file rejection, fresh setup reload with different setup country options and reopening the saved pitch page.

The SDL/Xvfb harness additionally saves/overwrites, shows info and reloads in the actual executable, then restarts the process with another startup seed and loads via setup. Before/after snapshots and native/controller screenshots remain in the private QA repository. Native Windows/macOS execution and complete career/release checks remain open.
