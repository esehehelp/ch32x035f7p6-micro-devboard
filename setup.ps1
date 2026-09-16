[CmdletBinding()]
param(
    [string]$Sketchbook,
    [switch]$SkipDriver,
    [switch]$SkipProjectTools
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Write-Step([string]$Message) {
    Write-Host "`n==> $Message" -ForegroundColor Cyan
}

function Get-SketchbookPath {
    if ($Sketchbook) { return [IO.Path]::GetFullPath($Sketchbook) }

    $cliConfig = Join-Path $env:USERPROFILE ".arduinoIDE\arduino-cli.yaml"
    if (Test-Path -LiteralPath $cliConfig) {
        $line = Get-Content -LiteralPath $cliConfig |
            Where-Object { $_ -match '^\s*user:\s*(.+?)\s*$' } |
            Select-Object -First 1
        if ($line -and $line -match '^\s*user:\s*["'']?(.+?)["'']?\s*$') {
            return [Environment]::ExpandEnvironmentVariables($Matches[1])
        }
    }

    $legacyPrefs = Join-Path $env:LOCALAPPDATA "Arduino15\preferences.txt"
    if (Test-Path -LiteralPath $legacyPrefs) {
        $line = Get-Content -LiteralPath $legacyPrefs |
            Where-Object { $_ -like 'sketchbook.path=*' } |
            Select-Object -First 1
        if ($line) { return $line.Substring('sketchbook.path='.Length) }
    }

    return Join-Path ([Environment]::GetFolderPath('MyDocuments')) "Arduino"
}

function Save-Download([string]$Uri, [string]$Destination, [string]$Sha256 = "") {
    Write-Host "Downloading $Uri"
    Invoke-WebRequest -UseBasicParsing -Uri $Uri -OutFile $Destination
    if ($Sha256) {
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Destination).Hash.ToLowerInvariant()
        if ($actual -ne $Sha256.ToLowerInvariant()) {
            throw "Checksum mismatch for $Uri`nexpected: $Sha256`nactual:   $actual"
        }
    }
}

function Get-GitHubAsset([string]$Repository, [string]$Tag, [string]$Name) {
    $headers = @{ 'User-Agent' = 'ch32x035f7p6-setup' }
    $release = Invoke-RestMethod -Headers $headers -Uri "https://api.github.com/repos/$Repository/releases/tags/$Tag"
    $asset = $release.assets | Where-Object { $_.name -eq $Name } | Select-Object -First 1
    if (-not $asset) { throw "Release asset not found: $Repository/$Tag/$Name" }
    $digest = if ($asset.digest -like 'sha256:*') { $asset.digest.Substring(7) } else { "" }
    return @{ Uri = $asset.browser_download_url; Sha256 = $digest }
}

function Test-Wchisp([string]$Executable) {
    try {
        $process = Start-Process -FilePath $Executable -ArgumentList '--version' -Wait -PassThru -WindowStyle Hidden
        return $process.ExitCode -eq 0
    }
    catch {
        return $false
    }
}

if (-not $IsWindows -and $PSVersionTable.PSVersion.Major -ge 6) {
    throw "setup.ps1 is for Windows. See README.md for other platforms."
}

$sourcePlatform = Join-Path $PSScriptRoot "arduino\ch32x035f7p6"
if (-not (Test-Path -LiteralPath (Join-Path $sourcePlatform "platform.txt"))) {
    throw "Run setup.ps1 from the repository root."
}

$sketchbookPath = Get-SketchbookPath
$installRoot = Join-Path $sketchbookPath "hardware\esehe\ch32x035f7p6"
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("ch32x035-setup-" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

try {
    Write-Step "Installing the Arduino platform"
    $sourceFullPath = [IO.Path]::GetFullPath($sourcePlatform).TrimEnd('\')
    $installFullPath = [IO.Path]::GetFullPath($installRoot).TrimEnd('\')
    if (-not $sourceFullPath.Equals($installFullPath, [StringComparison]::OrdinalIgnoreCase)) {
        New-Item -ItemType Directory -Force -Path $installRoot | Out-Null
        Copy-Item -Recurse -Force -Path (Join-Path $sourcePlatform '*') -Destination $installRoot
    }
    else {
        Write-Host "Platform is already in the Arduino sketchbook; skipping the copy."
    }

    Write-Step "Installing the RISC-V compiler"
    $indexUrl = "https://raw.githubusercontent.com/openwch/board_manager_files/main/package_ch32v_index.json"
    $index = Invoke-RestMethod -Uri $indexUrl
    $gccTool = $index.packages[0].tools |
        Where-Object { $_.name -eq 'riscv-none-embed-gcc' -and $_.version -eq '8.2.0' } |
        Select-Object -First 1
    $gccSystem = $gccTool.systems | Where-Object { $_.host -eq 'i686-mingw32' } | Select-Object -First 1
    if (-not $gccSystem) { throw "Windows RISC-V toolchain was not found in the WCH package index." }
    $gccZip = Join-Path $tempRoot $gccSystem.archiveFileName
    $gccHash = $gccSystem.checksum -replace '^SHA-256:', ''
    Save-Download $gccSystem.url $gccZip $gccHash
    $gccStage = Join-Path $tempRoot "gcc"
    Expand-Archive -LiteralPath $gccZip -DestinationPath $gccStage -Force
    $gccExe = Get-ChildItem -LiteralPath $gccStage -Recurse -Filter 'riscv-none-embed-gcc.exe' |
        Select-Object -First 1
    if (-not $gccExe) { throw "riscv-none-embed-gcc.exe was not found in the downloaded archive." }
    $gccRoot = Split-Path (Split-Path $gccExe.FullName -Parent) -Parent
    $gccDestination = Join-Path $installRoot "tools\gcc"
    New-Item -ItemType Directory -Force -Path $gccDestination | Out-Null
    Copy-Item -Recurse -Force -Path (Join-Path $gccRoot '*') -Destination $gccDestination

    Write-Step "Installing the uploader"
    $wchispAsset = Get-GitHubAsset 'ch32-rs/wchisp' 'nightly' 'wchisp-win-x64.zip'
    $wchispZip = Join-Path $tempRoot "wchisp.zip"
    Save-Download $wchispAsset.Uri $wchispZip $wchispAsset.Sha256
    $wchispStage = Join-Path $tempRoot "wchisp"
    Expand-Archive -LiteralPath $wchispZip -DestinationPath $wchispStage -Force
    $wchispExe = Get-ChildItem -LiteralPath $wchispStage -Recurse -Filter 'wchisp.exe' | Select-Object -First 1
    if (-not $wchispExe) { throw "wchisp.exe was not found in the downloaded archive." }
    $toolsDestination = Join-Path $installRoot "tools"
    Copy-Item -Force -LiteralPath $wchispExe.FullName -Destination (Join-Path $toolsDestination "wchisp.exe")

    $installedWchisp = Join-Path $toolsDestination "wchisp.exe"
    if (-not (Test-Wchisp $installedWchisp)) {
        Write-Step "Installing the Microsoft Visual C++ runtime required by wchisp"
        $vcRedist = Join-Path $tempRoot "vc_redist.x64.exe"
        Save-Download "https://aka.ms/vs/17/release/vc_redist.x64.exe" $vcRedist
        $vcSignature = Get-AuthenticodeSignature -LiteralPath $vcRedist
        if ($vcSignature.Status -ne 'Valid' -or $vcSignature.SignerCertificate.Subject -notmatch 'Microsoft') {
            throw "The Visual C++ runtime installer does not have a valid Microsoft signature."
        }
        $vcProcess = Start-Process -FilePath $vcRedist -ArgumentList '/install', '/quiet', '/norestart' -Verb RunAs -Wait -PassThru
        if ($vcProcess.ExitCode -notin 0, 1638, 3010) {
            throw "Visual C++ runtime setup failed with exit code $($vcProcess.ExitCode)."
        }
        if (-not (Test-Wchisp $installedWchisp)) {
            throw "wchisp still cannot start after installing the Visual C++ runtime."
        }
    }

    # PlatformIO finds these tools even when the installer is run from an
    # archive that is removed after setup, or the project is cloned later.
    $sharedToolsDestination = Join-Path $env:LOCALAPPDATA "CH32X035\tools"
    New-Item -ItemType Directory -Force -Path $sharedToolsDestination | Out-Null
    Copy-Item -Force -LiteralPath $wchispExe.FullName -Destination (Join-Path $sharedToolsDestination "wchisp.exe")

    if (-not $SkipProjectTools) {
        $pioToolsDestination = Join-Path $PSScriptRoot "firmware\tools"
        New-Item -ItemType Directory -Force -Path $pioToolsDestination | Out-Null
        Copy-Item -Force -LiteralPath $wchispExe.FullName -Destination (Join-Path $pioToolsDestination "wchisp.exe")
    }

    # Current wchisp can use WCH's signed CH375 driver directly. Keeping this
    # DLL beside wchisp removes the old need to swap the device to WinUSB.
    $dllAsset = Get-GitHubAsset 'MeowKJ/BinaryKeyboard' 'toolchain-linux' 'CH375DLL64.dll'
    $dllPath = Join-Path $toolsDestination "CH375DLL64.dll"
    Save-Download $dllAsset.Uri $dllPath $dllAsset.Sha256
    Copy-Item -Force -LiteralPath $dllPath -Destination (Join-Path $sharedToolsDestination "CH375DLL64.dll")
    if (-not $SkipProjectTools) {
        Copy-Item -Force -LiteralPath $dllPath -Destination (Join-Path $pioToolsDestination "CH375DLL64.dll")
    }

    if (-not $SkipDriver) {
        Write-Step "Installing the signed WCH USB ISP driver (a UAC prompt may appear)"
        $driverSfx = Join-Path $tempRoot "CH372DRV.EXE"
        $driverUrl = "https://raw.githubusercontent.com/wagiminator/C64-Collection/master/C64_DiskMaster64/software/pc/tools/ch372_driver/CH372DRV.EXE"
        $driverHash = "a3c73528c549a08fead6f72f6e6afac21e97aa16badb6eef530a6b14600ed219"
        Save-Download $driverUrl $driverSfx $driverHash

        $signature = Get-AuthenticodeSignature -LiteralPath $driverSfx
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Qinheng') {
            throw "The WCH driver package does not have the expected valid Qinheng signature."
        }

        $driverStage = Join-Path $tempRoot "driver"
        New-Item -ItemType Directory -Force -Path $driverStage | Out-Null
        & tar.exe -xf $driverSfx -C $driverStage
        if ($LASTEXITCODE -ne 0) { throw "Windows tar could not extract the WCH driver package." }
        $driverSetup = Join-Path $driverStage "DRVSETUP64\DRVSETUP64.exe"
        if (-not (Test-Path -LiteralPath $driverSetup)) { throw "WCH driver installer was not found." }
        $process = Start-Process -FilePath $driverSetup -ArgumentList '/S' -Verb RunAs -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "WCH driver setup failed with exit code $($process.ExitCode)." }
    }

    Write-Step "Setup complete"
    Write-Host "Installed to: $installRoot" -ForegroundColor Green
    Write-Host "Restart Arduino IDE, select 'CH32X035F7P6 Micro Devboard', then open an example from File > Examples > CH32X035 Examples."
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
