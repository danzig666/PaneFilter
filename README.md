PaneFilter
==========

PaneFilter is a FAR Manager 3 plugin that opens a filterable plugin panel for
the current directory instead of a separate dialog.

Installation
------------

1. Copy the `PaneFilter` folder from the release zip (x64, or x86 for 32-bit
   FAR) into `%FARPROFILE%\Plugins` (usually
   `%APPDATA%\Far Manager\Profile\Plugins`). That folder needs no admin
   rights, which lets the plugin update itself. `%FARHOME%\Plugins` works
   too, but then updates have to be installed by hand.
2. Copy `PaneFilter macro.lua` to `%APPDATA%\Far Manager\Profile\Macros\scripts`
   to start PaneFilter by pressing `Alt+<letter>` in a file panel.

Features
--------

- Accepts a startup filter from `Plugin.Call(...)` / `Plugin.Command(...)`
  macro arguments, either a key name such as `AltA` or a filter text.
- Can be started from the command line: `panefilter:<filter>`.
- Filters live inside a FAR file panel.
- Supports substring matching and `*` / `?` wildcard matching, ignoring case
  and accents (`e` finds `é`, `o` finds `ő`, `l` finds `ł`).
- After each change of the filter the cursor goes to the first item in the
  panel's sort order, e.g. the newest file when the panel is sorted by time.
- Shows file name, size, and modified date/time in the panel columns.
- `Del` jumps to the currently selected file in the real panel.
- `Enter` on a directory closes the plugin and switches the real panel to that directory.
- `Enter` on a file positions the real panel on it and opens it.
- `Esc` returns to the original directory and item.

Updating
--------

F11 > **PaneFilter: Check for updates** asks GitHub for the latest release.
If there is a newer one, **Update now** downloads the DLL and the language and
help files, checks their SHA-256 checksums, and swaps them in while FAR keeps
running (Windows lets a loaded DLL be renamed, not overwritten). Restart FAR to
use the new version; FAR windows that are open keep the old one until they
restart. The old DLL is kept as `PaneFilter.dll.<version>-<time>.old` and
deleted automatically, at FAR start or after the next update, once no running
FAR has it loaded.

The updater is shared with
[far-github-panel](https://github.com/danzig666/far-github-panel)
(`src/Updater.cpp`; per-plugin settings in `src/UpdaterConfig.hpp`).

Build
-----

Every push is built for x86 and x64 by GitHub Actions
(`.github/workflows/build.yml`); the binaries are attached to the run as
artifacts.

Locally, run `vc.build.release.bat` from this directory. It builds x86 and x64
variants and places the results in `bin\x86` and `bin\x64`.

Releases
--------

Pushing a tag named `vX.Y.Z`, or running the Build workflow manually with
its `tag` input set, builds both variants and publishes a GitHub release with
ready-to-install zips (plugin files, macro, README) and the bare `PaneFilter.x86.dll`, `PaneFilter.x64.dll`,
`PaneFilter_Eng.lng` and `PaneFilter_Eng.hlf` with `.sha256` checksums, which
the updater downloads. Keep `src/version.hpp` in sync with the tag.
