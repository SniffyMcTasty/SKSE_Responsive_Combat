param(
    [ValidateSet('debug', 'release')]
    [string]$Configuration = 'debug',
    [switch]$Deploy
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$originalEnvironment = @{}
Get-ChildItem Env: | ForEach-Object { $originalEnvironment[$_.Name] = $_.Value }

Push-Location $projectRoot
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'Install Visual Studio Build Tools with the Desktop development with C++ workload.'
    }
    $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw 'No Visual Studio installation with the x64 C++ tools was found.'
    }

    $devCmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
    $developerEnvironment = & $env:ComSpec /d /c "call `"$devCmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the Visual Studio developer environment.' }
    foreach ($line in $developerEnvironment) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
    if ($originalEnvironment.ContainsKey('VCPKG_ROOT')) {
        $env:VCPKG_ROOT = $originalEnvironment['VCPKG_ROOT']
    }

    $cmakeTools = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    $env:PATH = "$(Join-Path $cmakeTools 'CMake\bin');$(Join-Path $cmakeTools 'Ninja');$env:PATH"
    $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
    if (-not $env:VCPKG_ROOT -or -not (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'))) {
        throw 'Set VCPKG_ROOT to your vcpkg checkout before building.'
    }

    # Verification builds should not automatically replace an installed mod.
    if (-not $Deploy) {
        Remove-Item Env:SKYRIM_FOLDER, Env:SKYRIM_MODS_FOLDER -ErrorAction SilentlyContinue
    }

    & $cmake --preset $Configuration
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & $cmake --build --preset $Configuration
    if ($LASTEXITCODE -ne 0) { throw 'CMake build failed.' }
}
finally {
    Pop-Location
    Get-ChildItem Env: | Where-Object { -not $originalEnvironment.ContainsKey($_.Name) } |
        ForEach-Object { [Environment]::SetEnvironmentVariable($_.Name, $null, 'Process') }
    foreach ($name in $originalEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $originalEnvironment[$name], 'Process')
    }
}
