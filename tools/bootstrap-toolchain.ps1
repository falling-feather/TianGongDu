[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$lockPath = Join-Path $root "toolchains\toolchain-lock.json"
$lock = Get-Content -Raw -Encoding UTF8 -LiteralPath $lockPath | ConvertFrom-Json
$rangeDownloader = Join-Path $PSScriptRoot "download-range.ps1"

function Resolve-RepositoryPath([string]$RelativePath) {
    if ([string]::IsNullOrWhiteSpace($RelativePath)) {
        throw "Expected a repository-relative path."
    }
    $absolute = [System.IO.Path]::GetFullPath((Join-Path $root $RelativePath))
    $prefix = $root.TrimEnd('\') + '\'
    if (-not $absolute.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes the repository: $RelativePath"
    }
    return $absolute
}

function Get-Sha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            $bytes = $sha256.ComputeHash($stream)
            return ([System.BitConverter]::ToString($bytes)).Replace('-', '').ToLowerInvariant()
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Get-ExpectedSha256($Entry) {
    if ($Entry.integrity -notmatch '^sha256:([0-9a-f]{64})$') {
        throw "Entry $($Entry.version) does not have a locked SHA-256."
    }
    return $Matches[1]
}

function Move-InvalidCache([string]$Path) {
    $invalidPath = "$Path.invalid-$([DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff'))"
    $resolvedParent = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetFullPath($invalidPath))
    $cacheRoot = Resolve-RepositoryPath $lock.policy.downloadsRoot
    $toolchainRoot = Resolve-RepositoryPath $lock.policy.cacheRoot
    if (-not $resolvedParent.StartsWith($cacheRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
        -not $resolvedParent.StartsWith($toolchainRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to move a cache outside locked cache roots: $Path"
    }
    Move-Item -LiteralPath $Path -Destination $invalidPath
    Write-Warning "Preserved invalid cache as $invalidPath"
}

function Ensure-CachedArtifact([string]$Name, $Entry) {
    if ([string]::IsNullOrWhiteSpace($Entry.cachePath)) {
        return
    }
    if ([string]::IsNullOrWhiteSpace($Entry.provenance.url)) {
        throw "$Name has a cache path but no source URL."
    }
    if ($null -eq $Entry.provenance.expectedSizeBytes) {
        throw "$Name has no locked byte length."
    }

    $path = Resolve-RepositoryPath $Entry.cachePath
    $expectedSize = [long]$Entry.provenance.expectedSizeBytes
    $expectedHash = Get-ExpectedSha256 $Entry

    if ([System.IO.File]::Exists($path)) {
        $length = ([System.IO.FileInfo]::new($path)).Length
        if ($length -eq $expectedSize) {
            $actualHash = Get-Sha256 $path
            if ($actualHash -eq $expectedHash) {
                Write-Output "verified cache: $Name"
                return
            }
            Move-InvalidCache $path
        } elseif ($length -gt $expectedSize) {
            Move-InvalidCache $path
        }
    }

    & $rangeDownloader `
        -Uri $Entry.provenance.url `
        -Destination $path `
        -StartOffset 0 `
        -EndOffset ($expectedSize - 1) `
        -ChunkSizeMiB 4

    if (([System.IO.FileInfo]::new($path)).Length -ne $expectedSize) {
        throw "$Name byte length mismatch after download."
    }
    $actualHash = Get-Sha256 $path
    if ($actualHash -ne $expectedHash) {
        throw "$Name SHA-256 mismatch: expected $expectedHash, actual $actualHash"
    }
    Write-Output "downloaded and verified cache: $Name"
}

function Expand-ZipIfMissing($Entry, [string]$Marker, [switch]$IntoInstallRoot) {
    $installRoot = Resolve-RepositoryPath $Entry.installRoot
    $markerPath = Join-Path $installRoot $Marker
    if (Test-Path -LiteralPath $markerPath) {
        Write-Output "installed: $($Entry.version) ($Marker)"
        return
    }
    $destination = if ($IntoInstallRoot) {
        $installRoot
    } else {
        [System.IO.Path]::GetDirectoryName($installRoot)
    }
    [System.IO.Directory]::CreateDirectory($destination) | Out-Null
    Expand-Archive -LiteralPath (Resolve-RepositoryPath $Entry.cachePath) -DestinationPath $destination -Force
    if (-not (Test-Path -LiteralPath $markerPath)) {
        throw "Archive $($Entry.cachePath) did not produce $markerPath"
    }
}

function Find-Gpg {
    $command = Get-Command gpg.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    $git = (Get-Command git.exe -ErrorAction Stop).Source
    $gitRoot = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetDirectoryName($git))
    $candidate = Join-Path $gitRoot "usr\bin\gpg.exe"
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Git GPG was not found at $candidate"
    }
    return $candidate
}

function Confirm-LlvmSignature {
    $gpg = Find-Gpg
    $gpgHome = Resolve-RepositoryPath ".cache/gnupg-llvm"
    [System.IO.Directory]::CreateDirectory($gpgHome) | Out-Null
    $keys = Resolve-RepositoryPath $lock.supportArtifacts.llvmReleaseKeys.cachePath
    $signature = Resolve-RepositoryPath $lock.supportArtifacts.llvmDetachedSignature.cachePath
    $binary = Resolve-RepositoryPath $lock.tools.clang.cachePath
    $fingerprint = "B6C8F98282B944E3B0D5C2530FC3042E345AD05D"

    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $importOutput = & $gpg --homedir $gpgHome --batch --no-autostart --import $keys 2>&1
        $importOutput | ForEach-Object { Write-Host $_ }
        $verifyOutput = & $gpg --homedir $gpgHome --batch --no-autostart --status-fd 1 --verify $signature $binary 2>&1
        $verifyCode = $LASTEXITCODE
        $verifyOutput | ForEach-Object { Write-Host $_ }
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($verifyCode -ne 0 -or ($verifyOutput -join "`n") -notmatch "VALIDSIG $fingerprint") {
        throw "LLVM detached signature validation failed."
    }
    Write-Output "verified LLVM signer: $fingerprint"
}

function Confirm-ExecutableHash([string]$Name, $Entry, [string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Name executable not found: $Path"
    }
    $expectedHash = Get-ExpectedSha256 $Entry
    $actualHash = Get-Sha256 $Path
    if ($actualHash -ne $expectedHash) {
        throw "$Name executable SHA-256 mismatch: expected $expectedHash, actual $actualHash"
    }
    if ($null -ne $Entry.provenance.expectedSizeBytes -and
        ([System.IO.FileInfo]::new($Path)).Length -ne [long]$Entry.provenance.expectedSizeBytes) {
        throw "$Name executable byte length does not match the lock."
    }
    Write-Output "verified executable: $Name"
}

function Find-MsvcCompiler($Entry) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere not found: $vswhere"
    }

    $prefix = "vswhere:"
    if (-not $Entry.executable.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "MSVC executable must use the vswhere: locator."
    }
    $relativeCompiler = $Entry.executable.Substring($prefix.Length).Replace('/', '\')
    if ([System.IO.Path]::IsPathRooted($relativeCompiler) -or $relativeCompiler.Split('\') -contains '..') {
        throw "MSVC executable locator must stay inside the Visual Studio installation."
    }

    $installationPath = & $vswhere `
        -latest `
        -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath |
        Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($installationPath)) {
        throw "Visual Studio with the C++ toolset was not found."
    }
    $compiler = Join-Path $installationPath $relativeCompiler
    if (-not (Test-Path -LiteralPath $compiler)) {
        throw "Locked MSVC x64 compiler was not found: $compiler"
    }
    return $compiler
}

Push-Location $root
try {
    Ensure-CachedArtifact "emsdkManager" $lock.supportArtifacts.emsdkManager
    Expand-ZipIfMissing $lock.supportArtifacts.emsdkManager "emsdk.bat"

    foreach ($property in $lock.supportArtifacts.PSObject.Properties) {
        Ensure-CachedArtifact "supportArtifacts.$($property.Name)" $property.Value
    }
    foreach ($property in $lock.tools.PSObject.Properties) {
        Ensure-CachedArtifact $property.Name $property.Value
    }

    Expand-ZipIfMissing $lock.supportArtifacts.sevenZip "7z.exe"
    Expand-ZipIfMissing $lock.tools.axmol "core\CMakeLists.txt"
    Expand-ZipIfMissing $lock.supportArtifacts.axslcc "axslcc.exe" -IntoInstallRoot
    Expand-ZipIfMissing $lock.tools.cmake "bin\cmake.exe"
    Expand-ZipIfMissing $lock.tools.ninja "ninja.exe" -IntoInstallRoot

    Confirm-LlvmSignature
    $llvmRoot = Resolve-RepositoryPath $lock.tools.clang.installRoot
    $clang = Join-Path $llvmRoot $lock.tools.clang.executable
    if (-not (Test-Path -LiteralPath $clang)) {
        [System.IO.Directory]::CreateDirectory($llvmRoot) | Out-Null
        $sevenZip = Join-Path (Resolve-RepositoryPath $lock.supportArtifacts.sevenZip.installRoot) "7z.exe"
        & $sevenZip x (Resolve-RepositoryPath $lock.tools.clang.cachePath) "-o$llvmRoot" -bsp1 -y
        if ($LASTEXITCODE -ne 0) {
            throw "LLVM extraction failed with exit code $LASTEXITCODE."
        }
    }

    $emsdkRoot = Resolve-RepositoryPath $lock.supportArtifacts.emsdkManager.installRoot
    $env:EMSDK_KEEP_DOWNLOADS = "1"
    & (Join-Path $emsdkRoot "emsdk.bat") install $lock.tools.emscripten.version
    if ($LASTEXITCODE -ne 0) {
        throw "emsdk install failed with exit code $LASTEXITCODE."
    }
    & (Join-Path $emsdkRoot "emsdk.bat") activate $lock.tools.emscripten.version
    if ($LASTEXITCODE -ne 0) {
        throw "emsdk activate failed with exit code $LASTEXITCODE."
    }

    $node = Join-Path (Resolve-RepositoryPath $lock.tools.node.installRoot) $lock.tools.node.executable
    $python = Join-Path (Resolve-RepositoryPath $lock.tools.python.installRoot) $lock.tools.python.executable
    Confirm-ExecutableHash "node" $lock.tools.node $node
    Confirm-ExecutableHash "python" $lock.tools.python $python
    Confirm-ExecutableHash "msvc" $lock.tools.msvc (Find-MsvcCompiler $lock.tools.msvc)

    & $node "tools/verify-toolchain.mjs" --cache
    if ($LASTEXITCODE -ne 0) {
        throw "Toolchain cache verification failed."
    }
    foreach ($command in @(
        [PSCustomObject]@{ Tool = "cmake"; Argument = "--version" },
        [PSCustomObject]@{ Tool = "ninja"; Argument = "--version" },
        [PSCustomObject]@{ Tool = "clang-cl"; Argument = "--version" },
        [PSCustomObject]@{ Tool = "emcc"; Argument = "--version" },
        [PSCustomObject]@{ Tool = "node"; Argument = "--version" },
        [PSCustomObject]@{ Tool = "python"; Argument = "--version" }
    )) {
        & $node "tools/run-toolchain.mjs" $command.Tool $command.Argument
        if ($LASTEXITCODE -ne 0) {
            throw "Locked tool version probe failed: $($command.Tool)"
        }
    }

    Write-Output "TianGongDu candidate toolchain bootstrap complete. No persistent system environment was modified."
} finally {
    Pop-Location
}
