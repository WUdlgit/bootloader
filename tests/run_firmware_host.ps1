$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testBuild = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $testBuild -Force | Out-Null
$wasmFile = Join-Path $testBuild 'firmware.wasm'
& clang --target=wasm32 -std=c99 -O1 -Wall -Wextra -Werror -nostdlib `
    "-I$PSScriptRoot/firmware_host" "-I$projectRoot/bootloader/app" "-I$projectRoot/bootloader/driver" "-I$projectRoot/bootloader/third_lib/crc" `
    "$projectRoot/bootloader/app/magic_header.c" "$projectRoot/bootloader/driver/stm32_flash.c" "$projectRoot/bootloader/third_lib/crc/crc32.c" "$PSScriptRoot/firmware_host/flash_model.c" `
    '-Wl,--no-entry' '-Wl,--export-all' '-Wl,--wrap=crc32' '-Wl,--initial-memory=135266304' -o $wasmFile
if ($LASTEXITCODE -ne 0) { throw '固件模块的主机构建失败' }
& node "$PSScriptRoot/firmware_host/check.cjs" $wasmFile
if ($LASTEXITCODE -ne 0) { throw '固件模块的主机检查失败' }
