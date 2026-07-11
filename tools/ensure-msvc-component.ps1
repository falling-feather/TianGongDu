param(
    [switch]$InstallIfMissing
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$lock = Get-Content -LiteralPath (Join-Path $root "toolchains\toolchain-lock.json") -Raw |
    ConvertFrom-Json
$entry = $lock.tools.msvc
$prefix = "vswhere:"

if (-not $entry.executable.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "MSVC executable must use the vswhere: locator."
}
if ([string]::IsNullOrWhiteSpace($entry.componentId)) {
    throw "MSVC componentId is missing from the toolchain lock."
}

$relativeCompiler = $entry.executable.Substring($prefix.Length).Replace('/', '\')
if ([System.IO.Path]::IsPathRooted($relativeCompiler) -or $relativeCompiler.Split('\') -contains '..') {
    throw "MSVC executable locator must stay inside the Visual Studio installation."
}

$installerRoot = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer"
$vswhere = Join-Path $installerRoot "vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere not found: $vswhere"
}

$installationPath = & $vswhere -latest -products * -property installationPath |
    Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($installationPath)) {
    throw "Visual Studio 2022 was not found."
}

$compiler = Join-Path $installationPath $relativeCompiler
if (Test-Path -LiteralPath $compiler) {
    Write-Output "locked MSVC component present: $($entry.componentId)"
    Write-Output "locked MSVC compiler present: $compiler"
    exit 0
}

if (-not $InstallIfMissing) {
    throw "Locked MSVC component is missing. Re-run with -InstallIfMissing only on an approved disposable runner."
}
if ($env:CI -ne "true") {
    throw "Automatic MSVC component installation is restricted to disposable CI runners."
}

$setup = Join-Path $installerRoot "setup.exe"
if (-not (Test-Path -LiteralPath $setup)) {
    $setup = Join-Path $installerRoot "vs_installer.exe"
}
if (-not (Test-Path -LiteralPath $setup)) {
    throw "Visual Studio installer was not found under $installerRoot."
}

$arguments = @(
    "modify",
    "--installPath", "`"$installationPath`"",
    "--add", $entry.componentId,
    "--quiet",
    "--norestart",
    "--nocache"
)
Write-Output "installing locked MSVC component on disposable CI runner: $($entry.componentId)"
$process = Start-Process `
    -FilePath $setup `
    -ArgumentList $arguments `
    -Wait `
    -PassThru `
    -WindowStyle Hidden
if ($process.ExitCode -notin @(0, 3010)) {
    throw "Visual Studio installer failed with exit code $($process.ExitCode)."
}
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Visual Studio installer completed, but the locked compiler is still missing: $compiler"
}
Write-Output "installed locked MSVC component: $($entry.componentId)"
exit 0
