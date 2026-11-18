# Skyrim Together Reborn on macOS (Apple Silicon, Wine)

This branch is Skyrim Together Reborn **v1.8.4** with the changes needed to build it on a Mac and run it under Wine,
plus client-side fixes collected from community forks.

- Game: Skyrim Special Edition **1.6.1170 (Steam) / 1.6.1179 (GOG)**, built with `--skyrim_version=1.6.1170`.
  The default (`latest`) targets 1.7.104.
- Network protocol and server: unchanged from v1.8.4. The build reports `v1.8.4` (`--build_version=v1.8.4`), so it
  plays with official v1.8.4 clients and servers on Windows. Use the official v1.8.4 server.
- Tested: GOG Skyrim 1.6.1179 in a Wine 10 prefix with DXMT (Metal) on an M4 MacBook Pro.

## Mac and Wine changes

| Commit | What it does |
|---|---|
| Loader: merge the game's and launcher's exception tables under Wine | Wine reads unwind data from the main module's in-memory headers. After the game's headers replace the launcher's, exceptions raised in launcher code could not be unwound and the launcher died at start-up. Runs only under Wine. |
| Save buffers: never resize the game's buffer from WriteId | Fixes a crash after creating a character with mods loaded: the writer freed the game's (stack) buffer through mimalloc. Same data format. |
| Mac cross builds: no OpenSSL, run-time fmt checks | OpenSSL's Windows build can't run on macOS, so HTTPS (only the server's public-list announcement) is left out there. |
| Build with clang-cl: standard C++ for MSVC-only constructs | Same behaviour, also accepted by MSVC. Includes the TiltedReverse fix (submodule now points at `HAB3X/TiltedReverse`, branch `mac`). |
| Animation graph holder: dispatch the game's virtuals through its vtable | Clang calls some game virtuals directly; these forward through the object's own (the game's) vtable, like an MSVC build. |

## Fixes from community forks

134 client-side commits from 26 contributors' forks, applied onto v1.8.4 with their original authorship. Changes to the
network protocol, the server and the shipped game files were left out to keep compatibility with v1.8.4; commits that
did not build together with the rest were dropped. The full list, with authors and dates, is in
[FORK-FIXES.md](FORK-FIXES.md). Thanks to everyone listed there.

## Building on macOS

Windows binaries are cross-compiled with LLVM's `clang-cl`/`lld-link` and Microsoft's CRT and Windows SDK (via
`xwin`), driven by xmake. The wrappers, patches and launch scripts are in
[HAB3X/skyrim-mac-tools](https://github.com/HAB3X/skyrim-mac-tools).

```bash
. ~/Developer/xwin/env.sh
xmake f -p windows -a x64 -m releasedbg --toolchain=clang-cl --sdk="$WINSYSROOT" --mrc=llvm-rc --skyrim_version=1.6.1170 --build_version=v1.8.4 -y
xmake build SkyrimImmersiveLauncher && xmake build TPProcess && xmake build ImmersiveElf
```

Install the official v1.8.4 release for 1.6.1170 into the game's `Data` folder, then replace `SkyrimTogether.exe`,
`EarlyLoad.dll` and `TPProcess.exe` in `Data/SkyrimTogetherReborn` with the ones from
`build/windows/x64/releasedbg`. The Address Library (`versionlib-1-6-1179-0.bin` for GOG) goes in
`Data/SKSE/Plugins`.

Licence: as the upstream project (see LICENSE).
