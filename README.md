# OpenBMH-Extended

**1.0 Beta — in development.** This is the extended branch of the original-game
reconstruction. The current shared German/English career implementation is still
being integrated; a successful build is not a playable Beta release.

The faithful Classic line remains **[OpenBMH 1.0.x](https://github.com/Weyoune/openbmh)**.
Use that repository for the original league structures and original-game community
maintenance. Extended development, its native data editor and future Beta releases
live here. See [the project split](docs/PROJECT_SPLIT.md).

## Scope and current state

OpenBMH-Extended retains the Classic SDL2 interface and shared match calculations,
while extending the database, squad sizes, foreign club crests and country support.
The first Beta targets one world for 1–4 managers with German and English clubs,
shared European competitions and separate national domestic cups. Its accepted
scope and remaining release gates are in [the implementation contract](docs/poc-1.2/IMPLEMENTATION.md)
and [status](docs/poc-1.2/STATUS.md).

The earlier OpenBMH 1.1 Betas are available as clearly marked historical releases.
They predate the shared-career implementation and are not the new 1.0 Beta.
The formerly named 1.2 development work continues on this repository's `main`.
Internal `world12` names and the existing typed save/data identifiers remain for
compatibility; the project split does not rewrite saved data.

No original executables, databases, artwork, music or saved games are distributed
in this public repository. Use your own supported data. Code is GPL-3.0-only;
third-party notices and asset rights remain applicable. This independent project
is not affiliated with the original rights holders or football organizations.

## Running development builds

The executable remains `OpenBMH.exe` on Windows and `OpenBMH` elsewhere. The CLI
remains `openbmh_cli`; the native Windows data editor is `obdeddi.exe`.

The shared-career development path requires an explicitly prepared Extended data
directory. Its temporary entry is `--extended-dev` (`--world12-dev` remains an
internal compatibility alias):

```sh
./OpenBMH --extended-dev --data "path/to/extended-data" --save-directory "path/to/saves"
```

The existing data-only community path and loose Classic reference installations
are retained during development. Original1.1 or Classic saved games are not
converted into shared-career snapshots. Keep saves and data backups separate.

## Build from source

The following recipes build OpenBMH-Extended. They require Git, CMake 3.24 or newer and a C++20 compiler. The first configure fetches pinned open-source dependencies. Native build and package results are documented for each release; these instructions do not claim an untested platform result.

Clone the public repository, then run the commands for your system:

```sh
git clone https://github.com/Weyoune/OpenBMH-extended.git
cd OpenBMH-extended
```

### Windows

Install Visual Studio 2022 or its Build Tools with **Desktop development with C++**, a Windows SDK and CMake. From Developer PowerShell, using the Visual Studio generator:

```powershell
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64
cmake --build build-windows --config Release
ctest --test-dir build-windows -C Release --output-on-failure
.\build-windows\Release\OpenBMH.exe --data "path/to/game" --save-directory "path/to/saves"
```

### Linux

On Debian or Ubuntu, install the compiler and development libraries. This example uses the default GTK3 file dialog backend:

```sh
sudo apt install build-essential git cmake ninja-build pkg-config libgtk-3-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libasound2-dev libpulse-dev libgl1-mesa-dev libudev-dev
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
./build-linux/OpenBMH --data "path/to/game" --save-directory "path/to/saves"
```

Use a newer CMake if your distribution ships a version older than 3.24. A Linux binary can still require system libraries such as GTK3; a ZIP is not a guarantee of portability across distributions.

### macOS

Apple's Command Line Tools supply the compiler and SDK; full Xcode is not required. Install CMake and Ninja separately, for example with Homebrew. On Apple Silicon:

```sh
xcode-select --install
brew install cmake ninja
cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-macos
ctest --test-dir build-macos --output-on-failure
./build-macos/OpenBMH --data "path/to/game" --save-directory "path/to/saves"
```

On Intel Macs omit the arm64 setting or select `x86_64`. These commands build terminal executables; they do not create a signed application bundle.

Game-data tests are optional: add `-DBMH_DATA="path/to/game"` when configuring to supply your own installation. The repository includes no original test data. See [BUILDING](docs/BUILDING.md) for test skips, system SDL2 and packaging details.

## Documentation and downloads

- [Extended development status](docs/poc-1.2/STATUS.md)
- [Project split and historical version names](docs/PROJECT_SPLIT.md)
- [Build details](docs/BUILDING.md), [architecture](docs/architecture.md)
- [Contributing](CONTRIBUTING.md), [compatibility decisions](docs/COMPATIBILITY.md)
- [Extended releases](https://github.com/Weyoune/OpenBMH-extended/releases)
- [Classic OpenBMH](https://github.com/Weyoune/openbmh)

Report Extended issues in this repository. Never attach original game files or
unredacted saved games to a public issue.

The supplied project start artwork is embedded at native resolution; see [start-screen artwork](docs/START_SCREEN.md). It replaces the original title image while the game continues to use your own original data and assets.
