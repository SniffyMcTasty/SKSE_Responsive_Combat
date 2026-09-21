# Responsive Combat

Responsive Combat is an SKSE/CommonLibSSE NG mod in development for configurable baseline attack speed, attack-to-block and attack-to-dodge cancellation, and control over unwanted queued attacks.

The first milestone adds startup configuration and persistent logging to the working plugin. Gameplay features are planned, not implemented. See the [incremental development plan](docs/ROADMAP.md) for milestones and acceptance checks.

The plugin initializes SKSE, reads configuration, logs startup diagnostics, and keeps the existing in-game console message once data has loaded:

```text
[ResponsiveCombat] Plugin loaded successfully.
```

## Gameplay Scope

Responsive Combat is intended as a gameplay supplement to Attack MCO/ADXP and Dodge MCO (DMCO), adding configurable attack speed, block and dodge cancellation, and more deliberate attack buffering to the existing combat system.

These gameplay features are planned. No combat hooks are installed yet; integration requirements and supported versions will be documented as each feature is implemented and tested.

## Requirements

- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.21 or newer
- Ninja
- vcpkg
- Skyrim Script Extender for the Skyrim runtime you are targeting
- Address Library for SKSE Plugins matching that runtime

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

From a regular PowerShell terminal, the build script locates Visual Studio's C++ tools, CMake, and Ninja, configures an x64 build, and runs the automated tests:

```powershell
.\scripts\build.ps1
.\scripts\build.ps1 -Configuration release
```

Install the C++ CMake tools component through Visual Studio Installer if CMake or Ninja is missing. The script uses `VCPKG_ROOT` and restores the terminal's environment when finished. No global PATH changes or local user preset are required for this workflow.

Alternatively, configure and build from a Visual Studio developer shell:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure --no-tests=error
```

The first configure step lets vcpkg download and build CommonLibSSE NG, [inih](https://github.com/benhoyt/inih), and spdlog with their dependencies.

CTest covers configuration defaults and validation, file handling, log levels, persistent output, fallback logging, rotation, and configuration-preserving deployment. These tests run without Skyrim. The build script fails if compilation or any test fails. Loading the DLL and checking gameplay behavior still requires Skyrim with SKSE.

## Deploying The Plugin

By default, build output stays inside the local build directory.

The PowerShell build script deploys only when `-Deploy` is supplied, after compilation and tests succeed:

```powershell
.\scripts\build.ps1 -Configuration release -Deploy
```

Direct CMake builds do not deploy automatically. When a deployment environment variable is present at configure time, `cmake --build --preset release --target deploy` explicitly deploys that build. Run CTest first when using this direct target.

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

The mod-manager destination takes precedence when both environment variables are set. Deployment includes the example INI only if none exists at the destination; existing settings are never overwritten. Debug deployment also includes the PDB. Enable the mod in the mod manager and launch Skyrim through SKSE in that profile.

## Configuration

The example is [config/ResponsiveCombat.ini](config/ResponsiveCombat.ini). For manual installation, place it beside `ResponsiveCombat.dll` in `Data/SKSE/Plugins`, including through the mod manager's virtual filesystem. The path is resolved from the running Skyrim executable, not the shell's working directory.

```ini
[General]
SchemaVersion=1
Enabled=true

[Logging]
LogLevel=info
```

Settings are read once at startup. Restart Skyrim after editing the INI; there is no live reload or MCM yet.

- `SchemaVersion`: currently `1`. A missing version assumes schema 1. An unsupported or invalid version rejects the entire file and uses defaults.
- `Enabled`: `true` or `false` (also `1` or `0`), default `true`. This is the master flag for future gameplay features. It does not unload the plugin or disable diagnostics. Both values leave combat unchanged in this milestone.
- `LogLevel`: `trace`, `debug`, `info`, `warn`, `error`, `critical`, or `off`; default `info`. Startup configuration and initialization diagnostics are always recorded, then this filter applies to subsequent messages, including data load.

Section names, keys, boolean values, and log levels are case-insensitive. Missing files or settings use defaults. Invalid individual values use their own defaults while retaining other valid settings. Unknown keys are ignored with a warning. Malformed INI syntax, duplicate keys, and multiline values reject the entire file. Diagnostics are emitted once during startup, and loading never creates or rewrites an INI.

Use UTF-8 text (an optional BOM is supported), with one setting per line and no indentation. Files are limited to 64 KiB and lines to 190 bytes, including any carriage return. Oversized input and NUL bytes are rejected. Full-line `;` or `#` comments and whitespace-prefixed inline `;` comments are supported.

## Logs And Verification

`ResponsiveCombat.log` is written under SKSE's resolved log directory, normally `Documents/My Games/Skyrim Special Edition/SKSE`. The actual location follows the runtime and Windows Documents folder configuration. The log records its own path, the plugin/runtime versions, configuration path and diagnostics, effective settings, initialization, and data load at the default verbosity.

Logs rotate at 1 MiB and at startup, retaining up to three previous files. If file logging cannot be initialized, diagnostics fall back to Windows debugger output and a warning appears in the in-game console after data load. A debugger or debug-output viewer is needed to read that fallback; it is not a persistent log file.

On Skyrim 1.6.1170, the pinned CommonLibSSE version can incorrectly resolve the game folder as `Skyrim.INI` ([upstream issue #98](https://github.com/CharmedBaryon/CommonLibSSE-NG/issues/98)). That specific result is corrected to `Skyrim Special Edition`, preserving the Windows Documents location, including OneDrive redirection. Logs from builds before this correction may remain under `Documents/My Games/Skyrim.INI/SKSE`; they are not moved or deleted automatically.

Milestone 1 is accepted following in-game verification on Skyrim 1.6.1170, including the corrected log location and configuration checks. Keep this checklist for regression testing:

1. Deploy the Release build and launch SKSE through the mod manager using a disposable test save.
2. Confirm the original console message appears and the startup log reaches `Game data loaded` with the default `LogLevel=info`.
3. Set `Enabled=false`, restart, and confirm the disabled console message and effective setting in the log. Combat should be unchanged.
4. Temporarily rename the installed INI, restart without redeploying, and confirm defaults are logged and no replacement file is created.
5. Restore the INI, try `Enabled=invalid`, restart, and confirm one invalid-value warning and `Enabled=true`. Restore the defaults afterward.

Keep generated directories and local configuration out of git. In particular, do not commit `build/`, `cmake-build*/`, `vcpkg_installed/`, `.idea/`, `.vscode/`, or `CMakeUserPresets.json`.
