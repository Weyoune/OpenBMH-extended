# Classic and Extended project split

| Project | Repository | Version line |
| --- | --- | --- |
| OpenBMH Classic | [Weyoune/openbmh](https://github.com/Weyoune/openbmh) | 1.0.x, original-game reconstruction |
| OpenBMH-Extended | [Weyoune/OpenBMH-extended](https://github.com/Weyoune/OpenBMH-extended) | 1.0 Beta, extended country/database development |

The prior OpenBMH 1.1 Beta releases and native `obdeddi` editor belong to Extended.
They are preserved as historical archives with their original tags, binary names,
checksums, corresponding source and original release notes. Archived build metadata
retains its original provenance. A historical1.1 package is not the new 1.0 Beta.
The former1.2 branch is now Extended `main`; its previous version number remains
in internal `world12` source paths, data profiles and save magic. These identifiers
are not the product's new release number and are intentionally not rewritten.

Development builds identify as 1.0.0-beta.1. This is a development version, not a
claim that the first playable Beta is finished. The shared-career release still
requires its documented integration, played-season and native GUI checks.
The executable remains `OpenBMH.exe`; new packages use the `OpenBMH-Extended-`
prefix. The native editor remains `obdeddi.exe` and a separate download.

The split preserves source history and attribution. Original game content and
private QA fixtures stay out of both public repositories. Classic releases and
their1.0.x source line remain in the Classic repository.
