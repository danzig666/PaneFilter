# PaneFilter

FAR Manager 3 plugin (Win32, MSVC, CMake). Sources are in `src/`, the FAR SDK headers in `farsdk/`.

## Building and verifying changes

The plugin only builds on Windows. Do not stop at "can't build here" in a Linux session:
push the branch and use the GitHub Actions workflow `.github/workflows/build.yml`
(x86 and x64, runs on every push) to compile it. Check the run and its job logs with
the GitHub tools and fix any errors before reporting the work as done.
