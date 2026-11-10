#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

/* 升级入口：处理命令、接收固件并启动有效 App；不返回 */
void bootloader_main(void) __attribute__((noreturn));

#endif
