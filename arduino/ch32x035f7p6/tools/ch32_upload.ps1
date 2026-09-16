param(
    [Parameter(Mandatory = $false)]
    [string]$Port,

    [Parameter(Mandatory = $true)]
    [string]$Firmware
)

$ErrorActionPreference = "Stop"
$wchisp = Join-Path $PSScriptRoot "wchisp.exe"

if (-not (Test-Path -LiteralPath $wchisp)) {
    Write-Error "wchisp.exe was not found. Run setup.bat again."
    exit 1
}
if (-not (Test-Path -LiteralPath $Firmware)) {
    Write-Error "Firmware file was not found: $Firmware"
    exit 1
}

# Do the touch here exactly once. Arduino's built-in touch is deliberately
# disabled because WCH BootROM is not a serial port and has no replacement COM.
if ($Port -and $Port -ne "none") {
    try {
        Write-Host "Triggering BootROM via 1200-bps touch on $Port"
        $serial = [System.IO.Ports.SerialPort]::new($Port, 1200)
        $serial.DtrEnable = $true
        $serial.Open()
        Start-Sleep -Milliseconds 50
        $serial.DtrEnable = $false
        $serial.Close()
        $serial.Dispose()
    }
    catch {
        Write-Warning "1200-bps touch failed ($($_.Exception.Message)); trying an existing BootROM device."
    }
}

Write-Host "Flashing $Firmware"
& $wchisp --usb --retry 8 flash $Firmware
exit $LASTEXITCODE
