/* Header for slim fat32 filesystem implementation */

#ifndef FS_SLIM32_H
#define FS_SLIM32_H

#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "lib/libc.h"
#include "lib/bitmap.h"

#define SLIM32_BYTS_PER_SEC	512
#define SLIM32_NUMBER_OF_FATS 2
#define SLIM32_SIGNATURE 0xAA55

/* Define size of a record in FAT32 */
#define SLIM32_RECORD_SIZE 32
#define SLIM32_VOLUMELABEL_SIZE 11

#define SLIM32_SHORT_FILENAME_SIZE 11

#define SLIM32_DIR_BITMASK 0x10
#define SLIM32_DIR_FLAG 1
#define SLIM32_DIR_NOT_FLAG 0

fs_t * slim32_init(gbd_t *disk);

int slim32_unmount(fs_t *fs);
int slim32_open(fs_t *fs, char *filename);
int slim32_close(fs_t *fs, int fileid);
int slim32_create(fs_t *fs, char *filename, int size);
int slim32_remove(fs_t *fs, char *filename);
int slim32_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset);
int slim32_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset);
int slim32_getfree(fs_t *fs);

#endif
