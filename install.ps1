# Bootstrap for machines without a local checkout. Download this file with
# curl.exe, then run it with powershell.exe -ExecutionPolicy Bypass -File.
[CmdletBinding()]
param(
    [string]$Sketchbook,
    [switch]$SkipDriver,
    [string]$Ref = 'fix/usb-c-pd-cdc-stability'
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

if (-not $Ref -or $Ref -notmatch '^[A-Za-z0-9._/-]+$' -or $Ref.Contains('..')) {
    throw 'Invalid repository ref.'
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('ch32x035-bootstrap-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $archive = Join-Path $tempRoot 'source.zip'
    $url = "https://api.github.com/repos/esehehelp/ch32x035f7p6-micro-devboard/zipball/$Ref"
    Write-Host "Downloading board sources from $url"
    Invoke-WebRequest -UseBasicParsing -Headers @{ 'User-Agent' = 'ch32x035f7p6-setup' } -Uri $url -OutFile $archive
    $sourceStage = Join-Path $tempRoot 'source'
    Expand-Archive -LiteralPath $archive -DestinationPath $sourceStage

    $setup = Get-ChildItem -LiteralPath $sourceStage -Directory |
        ForEach-Object { Join-Path $_.FullName 'setup.ps1' } |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
    if (-not $setup) { throw 'setup.ps1 was not found in the downloaded sources.' }

    # The extracted checkout is temporary: install shared tools in the user
    # profile so PlatformIO works even after this directory is removed.
    & $setup -Sketchbook $Sketchbook -SkipDriver:$SkipDriver -SkipProjectTools
    if (-not $?) { throw 'Board setup failed.' }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
