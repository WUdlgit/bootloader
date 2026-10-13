param([string]$PortName, [switch]$NoFlash)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$probePath = 'C:\Users\27366\AppData\Local\Microsoft\WinGet\Packages\xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe\xpack-openocd-0.12.0-7\bin\openocd.exe'
$runTag = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $PSScriptRoot "binary-rx-verification-$runTag.log"
$backupPath = "flash-before-binary-rx-$runTag.bin"
$appBefore = "app-before-binary-rx-$runTag.bin"
$appAfter = "app-after-binary-rx-$runTag.bin"
$serialPorts = @()
[IO.File]::WriteAllText($logPath, '', [Text.Encoding]::UTF8)
function Record([string]$Message) {
    [IO.File]::AppendAllText($logPath, $Message + [Environment]::NewLine, [Text.Encoding]::UTF8)
    if ($Message.Length -le 700) { Write-Host $Message }
    else { Write-Host ($Message.Substring(0, 200) + ' ... [full output in log]') }
}
function Board([string]$Commands) {
    $probeOutput = & $probePath -f interface/stlink.cfg -c 'transport select swd' -f target/stm32f4x.cfg -c $Commands 2>&1
    $probeExit = $LASTEXITCODE
    Record (($probeOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine)
    if ($probeExit -ne 0) { throw "OpenOCD failed: $probeExit" }
}
function ReadUntil([IO.Ports.SerialPort]$Port, [string]$Needle, [int]$TimeoutMs = 5000) {
    $text = ''
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.ElapsedMilliseconds -lt $TimeoutMs) {
        $text += $Port.ReadExisting()
        if ($text.Contains($Needle)) { Record $text; return $text }
        Start-Sleep -Milliseconds 10
    }
    Record $text
    throw "Timed out waiting for: $Needle"
}
function ReceiveCase([byte[]]$Bytes, [string]$Name, [bool]$SplitHeader = $false) {
    Record "CASE: $Name; size=$($Bytes.Length)"
    $activePort.DiscardInBuffer()
    $activePort.Write('u')
    $null = ReadUntil $activePort "已进入升级模式`r`n"
    if ($SplitHeader) {
        foreach ($character in "SIZE=$($Bytes.Length)`n".ToCharArray()) {
            $activePort.Write([string]$character)
            Start-Sleep -Milliseconds 20
        }
    } else { $activePort.Write("SIZE=$($Bytes.Length)`n") }
    $null = ReadUntil $activePort "size = $($Bytes.Length)`r`n"
    $activePort.Write($Bytes, 0, $Bytes.Length)
    $expectedHex = ($Bytes | ForEach-Object { $_.ToString('X2') }) -join ' '
    $null = ReadUntil $activePort "received = $($Bytes.Length)`r`n$expectedHex`r`n"
    Record "PASS: $Name"
}
try {
    $candidates = if ($PortName) { @($PortName) } else { @([IO.Ports.SerialPort]::GetPortNames()) }
    foreach ($candidate in $candidates) {
        $candidatePort = [IO.Ports.SerialPort]::new($candidate, 115200, [IO.Ports.Parity]::None, 8, [IO.Ports.StopBits]::One)
        $candidatePort.Encoding = [Text.Encoding]::UTF8
        $candidatePort.DtrEnable = $false
        $candidatePort.RtsEnable = $false
        $candidatePort.WriteTimeout = 2000
        try {
            $candidatePort.Open()
            $candidatePort.DiscardInBuffer()
            $serialPorts += $candidatePort
        } catch { Record "Cannot open ${candidate}: $($_.Exception.Message)"; $candidatePort.Dispose() }
    }
    if ($serialPorts.Count -eq 0) { throw 'No serial port could be opened' }
    if ($NoFlash) { Board 'init; reset run; shutdown' }
    else {
        Record 'Back up Flash before changing the board'
        Board "init; reset halt; dump_image $backupPath 0x08000000 0x80000; dump_image $appBefore 0x08040000 0x40000; reset run; shutdown"
    }
    Start-Sleep -Milliseconds 500
    $matches = @()
    foreach ($candidatePort in $serialPorts) {
        $startup = $candidatePort.ReadExisting()
        Record "STARTUP on $($candidatePort.PortName): $startup"
        if ($startup.Contains('[BOOT] bare-metal base ready')) { $matches += $candidatePort }
    }
    if ($matches.Count -ne 1) { throw "Expected one Bootloader serial port, found $($matches.Count)" }
    $activePort = $matches[0]
    Record "Selected $($activePort.PortName) from observed Bootloader startup"
    foreach ($candidatePort in $serialPorts) {
        if ($candidatePort -ne $activePort) { $candidatePort.Close() }
    }
    if (-not $NoFlash) {
        $activePort.DiscardInBuffer()
        Board "program bootloader/build/bootloader.elf verify; reset halt; dump_image $appAfter 0x08040000 0x40000; reset run; shutdown"
        if ((Get-FileHash $appBefore).Hash -ne (Get-FileHash $appAfter).Hash) { throw 'App Flash changed unexpectedly' }
        Record 'PASS: App Flash is unchanged after Bootloader programming'
        $null = ReadUntil $activePort '[BOOT] bare-metal base ready'
    }
    ReceiveCase ([byte[]](0x41, 0x00, 0x0A, 0xFF)) 'binary bytes including NUL/LF/FF'
    ReceiveCase ([byte[]](0xFF, 0x0A, 0x00, 0x41)) 'repeat reception resets count' $true
    $fullBuffer = [byte[]]::new(1024)
    for ($index = 0; $index -lt $fullBuffer.Length; $index++) { $fullBuffer[$index] = [byte]($index % 256) }
    ReceiveCase $fullBuffer 'full 1024-byte buffer, all byte values'
    foreach ($badSize in @(0, 1025)) {
        Record "CASE: reject size=$badSize before body"
        $activePort.DiscardInBuffer()
        $activePort.Write('u')
        $null = ReadUntil $activePort "已进入升级模式`r`n"
        $activePort.Write("SIZE=$badSize`n")
        $null = ReadUntil $activePort '[BOOT] receive failed'
        Record "PASS: size=$badSize rejected"
    }
    Record 'CASE: reject invalid length header'
    $activePort.DiscardInBuffer()
    $activePort.Write('u')
    $null = ReadUntil $activePort "已进入升级模式`r`n"
    $activePort.Write("BAD`n")
    $null = ReadUntil $activePort '[BOOT] invalid size'
    Record 'PASS: invalid header rejected'
    ReceiveCase ([byte[]](0x00, 0xFF, 0x0A, 0x41)) 'valid reception after rejected requests'
    Record 'CASE: existing g command starts App'
    $activePort.Write('g')
    $null = ReadUntil $activePort 'Hello world'
    Record 'PASS: App handoff greeting'
    Record 'BINARY RECEIVE CHECKS PASSED'
} catch {
    Record "FAILED: $($_.Exception.Message)"
    throw
} finally {
    foreach ($candidatePort in $serialPorts) {
        if ($candidatePort.IsOpen) { $candidatePort.Close() }
        $candidatePort.Dispose()
    }
    Write-Output "Verification log: $logPath"
}
