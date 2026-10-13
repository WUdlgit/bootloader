$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$openocd = 'C:\Users\27366\AppData\Local\Microsoft\WinGet\Packages\xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe\xpack-openocd-0.12.0-7\bin\openocd.exe'
$log = Join-Path $PSScriptRoot 'mode-select-serial-20261011.log'
[IO.File]::WriteAllText($log, '', [Text.Encoding]::UTF8)
function Record([string]$message) {
    Write-Output $message
    [IO.File]::AppendAllText($log, $message + [Environment]::NewLine, [Text.Encoding]::UTF8)
}
function Board([string]$commands) {
    # Windows PowerShell wraps native stderr in ErrorRecords, including OpenOCD info.
    $ErrorActionPreference = 'Continue'
    $output = & $openocd -f interface/stlink.cfg -c 'transport select swd' -f target/stm32f4x.cfg -c $commands 2>&1
    $code = $LASTEXITCODE
    $message = ($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    Record $message
    if ($code -ne 0) { throw "OpenOCD failed with exit code $code" }
}
$port = New-Object System.IO.Ports.SerialPort 'COM11', 115200, ([IO.Ports.Parity]::None), 8, ([IO.Ports.StopBits]::One)
$port.Encoding = [Text.Encoding]::UTF8
$port.DtrEnable = $false
$port.RtsEnable = $false
$port.ReadTimeout = 1000
$port.WriteTimeout = 1000
try {
    $port.Open()
    $port.DiscardInBuffer()
    Record 'RESET: expecting Bootloader output and no App output'
    Board 'init; reset run; sleep 500; shutdown'
    Start-Sleep -Milliseconds 200
    $boot = $port.ReadExisting()
    Record $boot
    if (-not $boot.Contains('[BOOT]') -or $boot.Contains('Hello world')) { throw 'Unexpected reset output' }

    Record 'SEND u'
    $port.Write('u')
    Start-Sleep -Milliseconds 400
    $upgrade = $port.ReadExisting()
    Record $upgrade
    $expected = -join ([char[]](0x5df2, 0x8fdb, 0x5165, 0x5347, 0x7ea7, 0x6a21, 0x5f0f))
    if (-not $upgrade.Contains($expected) -or $upgrade.Contains('Hello world')) { throw 'Unexpected upgrade-mode output' }
    Board 'init; halt; echo AFTER_U; echo [reg pc]; echo [mdw 0xe000ed08 1]; resume; shutdown'

    Record 'SEND g'
    $port.Write('g')
    Start-Sleep -Milliseconds 1400
    $app = $port.ReadExisting()
    Record $app
    if (-not $app.Contains('Hello world')) { throw 'No App greeting received' }
    Board 'init; halt; echo AFTER_G; echo [reg pc]; echo [mdw 0xe000ed08 1]; echo [mdw 0x20000004 1]; resume; sleep 1100; halt; echo APP_IRQ_SECOND_SAMPLE; echo [mdw 0x20000004 1]; resume; shutdown'
    Record 'SERIAL CHECKS PASSED'
} finally {
    if ($port.IsOpen) { $port.Close() }
    $port.Dispose()
}
