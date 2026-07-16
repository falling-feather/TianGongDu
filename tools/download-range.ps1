[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [uri]$Uri,

    [Parameter(Mandatory = $true)]
    [string]$Destination,

    [Parameter(Mandatory = $true)]
    [long]$StartOffset,

    [Parameter(Mandatory = $true)]
    [long]$EndOffset,

    [ValidateRange(1, 64)]
    [int]$ChunkSizeMiB = 4
)

$ErrorActionPreference = "Stop"

if ($EndOffset -lt $StartOffset) {
    throw "EndOffset must be greater than or equal to StartOffset."
}

$destinationPath = [System.IO.Path]::GetFullPath($Destination)
$destinationDirectory = [System.IO.Path]::GetDirectoryName($destinationPath)
if (-not [System.IO.Directory]::Exists($destinationDirectory)) {
    [System.IO.Directory]::CreateDirectory($destinationDirectory) | Out-Null
}

$expectedLength = $EndOffset - $StartOffset + 1
$currentLength = if ([System.IO.File]::Exists($destinationPath)) {
    ([System.IO.FileInfo]::new($destinationPath)).Length
} else {
    0L
}

if ($currentLength -gt $expectedLength) {
    throw "Existing destination is larger than the requested range: $currentLength > $expectedLength"
}

$chunkSize = [long]$ChunkSizeMiB * 1MB
while ($currentLength -lt $expectedLength) {
    $rangeStart = $StartOffset + $currentLength
    $rangeEnd = [Math]::Min($rangeStart + $chunkSize - 1, $EndOffset)
    $segmentLength = $rangeEnd - $rangeStart + 1
    $segmentPath = "$destinationPath.segment"

    & curl.exe `
        -L `
        --fail `
        --retry 20 `
        --retry-all-errors `
        --retry-delay 2 `
        --connect-timeout 20 `
        --max-time 300 `
        --silent `
        --show-error `
        --range "$rangeStart-$rangeEnd" `
        --output $segmentPath `
        $Uri.AbsoluteUri

    if ($LASTEXITCODE -ne 0) {
        throw "curl failed for byte range $rangeStart-$rangeEnd with exit code $LASTEXITCODE."
    }

    $actualSegmentLength = ([System.IO.FileInfo]::new($segmentPath)).Length
    if ($actualSegmentLength -ne $segmentLength) {
        throw "Range $rangeStart-$rangeEnd returned $actualSegmentLength bytes; expected $segmentLength."
    }

    $destinationStream = [System.IO.File]::Open(
        $destinationPath,
        [System.IO.FileMode]::Append,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::Read
    )
    $segmentStream = [System.IO.File]::OpenRead($segmentPath)
    try {
        $segmentStream.CopyTo($destinationStream)
    } finally {
        $segmentStream.Dispose()
        $destinationStream.Dispose()
    }
    Remove-Item -LiteralPath $segmentPath -Force

    $currentLength += $segmentLength
    Write-Output "downloaded $currentLength/$expectedLength bytes for $Destination"
}

Write-Output "range complete: $Destination ($currentLength bytes)"
