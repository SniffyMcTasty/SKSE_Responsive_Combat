# Responsive Combat SKSE Template

Responsive Combat is a small CommonLibSSE NG starter project for SKSE plugins.
The repository is meant to be a clean base for Skyrim combat experiments, not a finished gameplay mod.

The current plugin only initializes SKSE and prints this message to the in-game console once data has loaded:

```text
[ResponsiveCombat] Plugin loaded successfully.
```

Use this repository when starting a new native SKSE plugin that needs a known-good CMake, vcpkg, and CommonLibSSE NG setup.

## Requirements

- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.21 or newer
- Ninja
- vcpkg
- Skyrim Script Extender for the Skyrim runtime you are targeting

Set a user or system environment variable named `VCPKG_ROOT` that points to your vcpkg checkout:

```text
VCPKG_ROOT=C:\path\to\vcpkg
```

## First-Time Setup

Clone the repository, then open it in Visual Studio, Visual Studio Code, CLion, or another CMake-aware C++ editor.

The shared presets in `CMakePresets.json` define `debug` and `release` builds. If your editor cannot find Ninja or you need machine-specific settings, create a local `CMakeUserPresets.json`. This file is intentionally ignored by git.

Example:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "debug-local",
      "displayName": "Debug Local",
      "inherits": "debug",
      "cacheVariables": {
        "CMAKE_MAKE_PROGRAM": "C:/path/to/ninja.exe"
      }
    }
  ]
}
```

From a regular PowerShell terminal, the build script locates Visual Studio's C++ tools, CMake, and Ninja and configures an x64 build:

```powershell
.\scripts\build.ps1
.\scripts\build.ps1 -Configuration release
```

Install the C++ CMake tools component through Visual Studio Installer if CMake or Ninja is missing. The script uses `VCPKG_ROOT` and restores the terminal's environment when finished. No global PATH changes or local user preset are required for this workflow.

Alternatively, configure and build from a Visual Studio developer shell:

```powershell
cmake --preset debug
cmake --build --preset debug
```

The first configure step lets vcpkg download and build CommonLibSSE NG.

Successful compilation checks the native plugin and its dependencies. There are no automated tests yet; loading the DLL and checking gameplay behavior still requires Skyrim with SKSE.

## Deploying The Plugin

By default, build output stays inside the local build directory.

The PowerShell build script disables automatic deployment unless `-Deploy` is supplied:

```powershell
.\scripts\build.ps1 -Configuration release -Deploy
```

Direct CMake builds use the deployment environment variables whenever they are set.

To deploy directly to a Skyrim install, set `SKYRIM_FOLDER` to the folder that contains `SkyrimSE.exe`:

```text
SKYRIM_FOLDER=C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition
```

To deploy into a mod manager's mods folder, set `SKYRIM_MODS_FOLDER`:

```text
SKYRIM_MODS_FOLDER=C:\Users\<user>\AppData\Local\ModOrganizer\Skyrim Special Edition\mods
```

When `SKYRIM_MODS_FOLDER` is set, the built DLL is copied to:

```text
%SKYRIM_MODS_FOLDER%\ResponsiveCombat\SKSE\Plugins\
```

## Starting A New Mod From This Template

For a new mod, update these files first:

- `CMakeLists.txt`: change the `project(...)` name. This controls the DLL name and deployment folder.
- `vcpkg.json`: change the package `name`.
- `plugin.cpp`: change the console message.
- `README.md`: describe the actual mod goal and setup notes.

Keep generated directories and local configuration out of git. In particular, do not commit `build/`, `cmake-build*/`, `vcpkg_installed/`, `.idea/`, `.vscode/`, or `CMakeUserPresets.json`.

To start an independent project from this template, copy the source files without the `.git` directory, initialize a new Git repository, and push its initial commit to an empty remote repository.
