PaneFilter
==========

PaneFilter is a FAR Manager 3 plugin that opens a filterable plugin panel for
the current directory instead of a separate dialog.

Copy `PaneFilter macro.lua` to `%APPDATA%\Roaming\Far Manager\Profile\Macros\scripts`.

Features
--------

- Accepts a startup filter from `Plugin.Call(...)` macro arguments.
- Filters live inside a FAR file panel.
- Supports case-insensitive substring matching and `*` / `?` wildcard matching.
- Shows file name, size, and modified date/time in the panel columns.
- `Del` jumps to the currently selected file in the panel.
- `Enter` on a directory closes the plugin and switches the real panel to that directory.
- `Enter` on a file executes/opens the file.

Build
-----

Run `vc.build.release.bat` from this directory. It builds x86 and x64 variants
and places the results in `bin\x86` and `bin\x64`.
