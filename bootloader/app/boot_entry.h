#ifndef BOOT_ENTRY_H
#define BOOT_ENTRY_H

#include <stdbool.h>

/* RTC BKP19R is reserved for the one-shot App -> Bootloader request. */
#define BOOT_REQUEST_MAGIC 0x49504155U

void boot_entry_init(void);
bool boot_entry_take_request(void);
bool boot_entry_key_pressed(void);

#endif
