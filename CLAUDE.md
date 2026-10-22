# PaneFilter

FAR Manager 3 plugin (Win32, MSVC, CMake). Sources are in `src/`, the FAR SDK headers in `farsdk/`.

## Building and verifying changes

The plugin only builds on Windows. Do not stop at "can't build here" in a Linux session:
push the branch and use the GitHub Actions workflow `.github/workflows/build.yml`
(x86 and x64, runs on every push) to compile it. Check the run and its job logs with
the GitHub tools and fix any errors before reporting the work as done.

## Releasing

Cloud sessions can only push their own branch, not tags. To release, merge to `main`, then
dispatch the `build.yml` workflow on `main` with the `tag` input (e.g. `v1.2.0`); the
`release` job creates the tag and the GitHub release with the zips. Bump `src/version.hpp` first.
