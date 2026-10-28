#ifndef CS_SAVE_IO_H
#define CS_SAVE_IO_H
#include <stddef.h>
#include <stdint.h>
#define CS_SAVE_BYTES 8192u
#define CS_SAVE_BANK_BYTES 16384u
#define CS_SAVE_FILE_BYTES 32768u
/* Backend operations return 1 on success, 0 on failure. They operate only on
 * this game's save file, with flash-style erase/write semantics. */
int cs_save_io_init(void);
int cs_save_io_read(size_t offset, void *data, size_t length);
int cs_save_io_write(size_t offset, const void *data, size_t length);
int cs_save_io_erase(size_t offset, size_t length);
void cs_save_io_error(void);
void cs_sram_init(void);
int cs_sram_ok(void);
#endif
