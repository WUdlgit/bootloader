# WT_FreeRTOS 一键格式化脚本
# 保留各文件原有的 GBK 或 UTF-8 编码，失败时不覆盖原文件。
# 先在临时 UTF-8 文件中格式化，最后按原编码写回并保留末尾空行。
#
# 用法（在工程根目录）：
#   .\format.ps1                 # 默认处理 app、driver 和 third_lib/crc
#   .\format.ps1 -Paths app      # 只格式化 app/
# 如果被执行策略挡住：
#   powershell -ExecutionPolicy Bypass -File .\format.ps1

param(
    [string[]]$Paths = @('app', 'driver', 'third_lib/crc')
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$style = Join-Path $root '.clang-format'
$formatter = (Get-Command clang-format -CommandType Application -ErrorAction Stop).Source
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)
$gbk = [System.Text.Encoding]::GetEncoding(
    936,
    [System.Text.EncoderFallback]::ExceptionFallback,
    [System.Text.DecoderFallback]::ExceptionFallback
)

$files = foreach ($p in $Paths) {
    Get-ChildItem -LiteralPath (Join-Path $root $p) -Recurse -File |
        Where-Object { $_.Extension -in '.c', '.h' }
}

$count = 0
$failed = 0
foreach ($f in $files) {
    $temporary = Join-Path ([System.IO.Path]::GetTempPath()) (
        [System.Guid]::NewGuid().ToString('N') + $f.Extension
    )
    $replacement = Join-Path $f.DirectoryName (
        '.' + [System.Guid]::NewGuid().ToString('N') + '.format.tmp'
    )
    try {
        $original = [System.IO.File]::ReadAllBytes($f.FullName)
        $hasBom = $original.Length -ge 3 -and
            $original[0] -eq 0xEF -and $original[1] -eq 0xBB -and $original[2] -eq 0xBF

        # 严格解码，禁止把不认识的字节静默替换成乱码。
        $encoding = $utf8
        if ($hasBom) {
            $text = $utf8.GetString($original, 3, $original.Length - 3)
        } else {
            try {
                $text = $utf8.GetString($original)
            } catch [System.Text.DecoderFallbackException] {
                $encoding = $gbk
                $text = $gbk.GetString($original)
            }
        }

        # clang-format 统一处理临时 UTF-8 内容，不直接改原文件。
        [System.IO.File]::WriteAllBytes($temporary, $utf8.GetBytes($text))
        & $formatter -i "--style=file:$style" $temporary
        if ($LASTEXITCODE -ne 0) {
            throw "clang-format 退出码为 $LASTEXITCODE"
        }

        $formatted = $utf8.GetString([System.IO.File]::ReadAllBytes($temporary))
        $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
        $normalized = $formatted.TrimEnd("`r", "`n") + $nl + $nl
        $output = $encoding.GetBytes($normalized)
        if ($hasBom) {
            $output = [byte[]](@(0xEF, 0xBB, 0xBF) + $output)
        }

        # 成功转换回原编码后才写回；相同内容不重复改文件。
        if ([Convert]::ToBase64String($original) -ne [Convert]::ToBase64String($output)) {
            # 先完整写入同目录临时文件，再替换，避免写入失败截断原文件。
            [System.IO.File]::WriteAllBytes($replacement, $output)
            [System.IO.File]::Replace($replacement, $f.FullName, [NullString]::Value)
        }
        $count++
    } catch {
        $failed++
        Write-Warning "未格式化 $($f.FullName)：$($_.Exception.Message)"
    } finally {
        if ([System.IO.File]::Exists($temporary)) {
            [System.IO.File]::Delete($temporary)
        }
        if ([System.IO.File]::Exists($replacement)) {
            [System.IO.File]::Delete($replacement)
        }
    }
}

Write-Output "已处理 $count 个文件（目录: $($Paths -join ', ')）"
if ($failed -gt 0) {
    throw "$failed 个文件格式化失败，请检查上面的错误；这些文件未被覆盖。"
}
