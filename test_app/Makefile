# shellcheck disable=SC2068,SC2276,SC2283,SC2285
# 说明: 本文件是 GNU Makefile 而非 shell 脚本; 以上 SC 码为 shellcheck 以 sh 模式
#       解析 make 语法(赋值/+=/配方)时产生的误报, 在此统一禁用。
# =====================================================================
# STM32F407VET6 Makefile  (GNU Arm Toolchain 14.2 + GNU Make)
# Independent Hello world App at 0x08040000; uses copied base drivers.
# Bootloader handoff is implemented separately.
#
# 用法:
#   make         编译, 生成 build/test_app.elf/.hex/.bin
#   make clean   删除 build 目录
#   make flash   用 ST-Link + OpenOCD 编译并烧录
# =====================================================================

# ------- 工具链(已加入 PATH) -------
CROSS_COMPILE ?= arm-none-eabi-
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size

# OpenOCD: 优先使用 PATH；否则自动发现 WinGet 安装的 xPack 版本。
# 仍可通过 make flash OPENOCD=/path/to/openocd 覆盖。
# Windows 原生 Make 没有 command/cygpath，因此使用 where 和原生路径；
# Unix/Git Bash 环境继续使用 command -v。
ifeq ($(OS),Windows_NT)
OPENOCD ?= $(shell where openocd 2>NUL)
else
OPENOCD ?= $(shell command -v openocd 2>/dev/null)
endif

# Some Windows GNU Make environments do not export OS=Windows_NT.
# If LOCALAPPDATA is available, still discover the user-local xPack install.
ifeq ($(strip $(OPENOCD)),)
ifneq ($(strip $(LOCALAPPDATA)),)
LOCALAPPDATA_MAKE := $(subst \,/,$(LOCALAPPDATA))
OPENOCD := $(firstword $(wildcard $(LOCALAPPDATA_MAKE)/Microsoft/WinGet/Packages/xpack-dev-tools.openocd-xpack_*/xpack-openocd-*/bin/openocd.exe))
ifeq ($(strip $(OPENOCD)),)
# GNU Make on this host does not expand the WinGet wildcard reliably.
# Keep the discovered user-local install as an overrideable fallback.
OPENOCD := $(LOCALAPPDATA_MAKE)/Microsoft/WinGet/Packages/xpack-dev-tools.openocd-xpack_Microsoft.Winget.Source_8wekyb3d8bbwe/xpack-openocd-0.12.0-7/bin/openocd.exe
endif
endif
endif
ifeq ($(strip $(OPENOCD)),)
OPENOCD := openocd
endif

# ------- 宿主系统文件操作 -------
# Windows 原生 GNU Make 可能显示 SHELL=sh.exe，但在找不到 sh.exe 时会改由 cmd.exe
# 执行配方；Git Bash 的 SHELL 是带目录的实际 sh.exe 路径。
# 因此同时检查 OS 和 SHELL 路径，而不是仅检查 OS=Windows_NT。
ifeq ($(OS),Windows_NT)
ifneq ($(findstring /,$(SHELL)),)
HOST_SHELL := sh
else
HOST_SHELL := cmd
endif
else
HOST_SHELL := sh
endif

ifeq ($(HOST_SHELL),cmd)
winpath = $(subst /,\,$1)
make-dir = if not exist "$(call winpath,$1)" mkdir "$(call winpath,$1)"
remove-dir = if exist "$(call winpath,$1)" rmdir /s /q "$(call winpath,$1)"
else
make-dir = mkdir -p "$1"
remove-dir = rm -rf "$1"
endif

# ------- 目标 -------
TARGET  := test_app
BUILD   := build

# ------- 芯片与编译选项 -------
MCU     := cortex-m4
DEFS    := -DSTM32F40_41xxx -DUSE_STDPERIPH_DRIVER -DHSE_VALUE=8000000
CPU     := -mcpu=$(MCU) -mthumb
FPU     := -mfpu=fpv4-sp-d16 -mfloat-abi=hard
OPT     := -Og -g                 # -g 生成调试信息(单步/看变量必需); 发布可改成 -O2
# -Wno-* 系列: 兼容教程代码的宽松写法(与 Keil 行为一致), 否则 GCC 14 会报错
CFLAGS  := $(CPU) $(FPU) $(OPT) $(DEFS) -std=gnu99 -Wall -ffunction-sections -fdata-sections -MMD -MP \
           -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-int-conversion

# ------- 链接选项 -------
LDSCRIPT := stm32f407vetx.ld
LDFLAGS  := $(CPU) $(FPU) -T $(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
            -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
            -Wl,-e,Reset_Handler

# ------- 递归收集 .c 文件 -------
rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

C_SOURCES := $(call rwildcard,app/,*.c) \
             $(call rwildcard,driver/,*.c) \
             $(call rwildcard,firmware/cmsis/device/,*.c) \
             $(call rwildcard,firmware/driver/src/,*.c)

ASM_SOURCES := firmware/cmsis/device/startup_stm32f40_41xxx_gcc.s
OBJS        := $(addprefix $(BUILD)/, $(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o))

# ------- 头文件路径 -------
INCDIRS := . app driver firmware/cmsis/core firmware/cmsis/device firmware/driver/inc \
           $(wildcard driver/*/)
INCFLAGS := $(addprefix -I, $(INCDIRS))

.PHONY: all clean flash compiledb

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

# 编译 C 文件
$(BUILD)/%.o: %.c
	@$(call make-dir,$(@D))
	$(CC) $(CFLAGS) $(INCFLAGS) -c $< -o $@

# 编译汇编文件
$(BUILD)/%.o: %.s
	@$(call make-dir,$(@D))
	$(CC) $(CFLAGS) $(INCFLAGS) -x assembler-with-cpp -c $< -o $@

# 链接
$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	$(SIZE) $@

# 生成 HEX / BIN
$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# 清理
clean:
	@$(call remove-dir,$(BUILD))

# 烧录 (ST-Link SWD); 如果是 J-Link, 把 interface/stlink.cfg 换成 interface/jlink.cfg
flash: all
	"$(OPENOCD)" -f interface/stlink.cfg -c "transport select swd" -f target/stm32f4x.cfg -c "program $(BUILD)/$(TARGET).hex reset exit"

# 生成 clangd 需要的 compile_commands.json (新增源文件后运行一次即可)
compiledb:
	python -m compiledb -f make -B -n --no-print-directory

# Recompile affected source files when headers change.
-include $(OBJS:.o=.d)
