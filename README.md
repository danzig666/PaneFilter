PaneFilter
==========

PaneFilter is a FAR Manager 3 plugin that opens a filterable plugin panel for
the current directory instead of a separate dialog.

Installation
------------

1. Copy the contents of `bin\x64` (or `bin\x86` for 32-bit FAR) into
   `%FARHOME%\Plugins\PaneFilter`.
2. Copy `PaneFilter macro.lua` to `%APPDATA%\Far Manager\Profile\Macros\scripts`
   to start PaneFilter by pressing `Alt+<letter>` in a file panel.

Features
--------

- Accepts a startup filter from `Plugin.Call(...)` / `Plugin.Command(...)`
  macro arguments, either a key name such as `AltA` or a filter text.
- Can be started from the command line: `panefilter:<filter>`.
- Filters live inside a FAR file panel.
- Supports case-insensitive substring matching and `*` / `?` wildcard matching.
- Shows file name, size, and modified date/time in the panel columns.
- `Del` jumps to the currently selected file in the real panel.
- `Enter` on a directory closes the plugin and switches the real panel to that directory.
- `Enter` on a file positions the real panel on it and opens it.
- `Esc` returns to the original directory and item.

Build
-----

Every push is built for x86 and x64 by GitHub Actions
(`.github/workflows/build.yml`); the binaries are attached to the run as
artifacts.

Locally, run `vc.build.release.bat` from this directory. It builds x86 and x64
variants and places the results in `bin\x86` and `bin\x64`.
