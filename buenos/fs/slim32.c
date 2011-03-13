/* Implementation of G4.2 SLIM32 */

#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "vm/pagepool.h"
#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "fs/slim32.h"
#include "lib/libc.h"
#include "lib/bitmap.h"

/**@name Slim FAT32 Filesystem
 *
 * This module contains implementation for a slim version of the FAT32 filesystem
 *
 * @{
 */

/* FAT32 layout information */
typedef struct {
	uint16_t	BytsPerSec;
	uint8_t		SecPerClus;
	uint16_t	RsvdSecCnt;
	uint8_t		NumFATs;
	uint32_t	FATSz32;
	uint32_t	RootClus;
	uint16_t	Signature;
} BPB_t;

/* The shit we actually might use */
typedef struct {
	uint16_t	fat_begin_lba;
	uint32_t	cluster_begin_lba;
	uint32_t	sectors_per_cluster;
	uint32_t	root_dir_first_cluster;
} slim32_t;

fs_t * slim32_init(gbd_t *disk) {
	uint32_t addr;
	fs_t *fs;
	gbd_request_t req;
	BPB_t *bpb;
	slim32_t *slim32;
	int r;
	
	addr = pagepool_get_phys_page();
	if(addr == 0) {
		kprintf("slim32_init: could not allocate memory.\n");
		return NULL;
	}
	addr = ADDR_PHYS_TO_KERNEL(addr);	// transform to vm address
	/* Read header block, and make sure this is tfs drive */
	req.block = 0;
	req.sem = NULL;
	req.buf = ADDR_KERNEL_TO_PHYS(addr);   /* disk needs physical addr */
	r = disk->read_block(disk, &req);
	if(r == 0) {
		pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
		kprintf("tfs_init: Error during disk read. Initialization failed.\n");
		return NULL; 
	}
		
	fs	= (fs_t *)addr;
	bpb = (BPB_t *)(addr + sizeof(fs_t));
	slim32 = (slim32_t *)(addr + sizeof(fs_t) + sizeof(BPB_t));
	
	bpb->BytsPerSec	= *(uint16_t *)(addr + 0x0B);
	bpb->SecPerClus	= *(uint8_t *)(addr + 0x0D);
	bpb->RsvdSecCnt	= *(uint16_t *)(addr + 0x0E);
	bpb->NumFATs	= *(uint8_t *)(addr + 0x10);
	bpb->FATSz32	= *(uint32_t *)(addr + 0x24);
	bpb->RootClus	= *(uint32_t *)(addr + 0x2C);
	bpb->Signature	= *(uint16_t *)(addr + 0x1FE);
	
	/* Check if Bytes Per Sector is set correct */
	if(bpb->BytsPerSec != SLIM32_BYTS_PER_SEC) {
		pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
		return NULL;
	}

	/* Check if Number of FATs is set correct */
	if(bpb->NumFATs != SLIM32_NUMBER_OF_FATS) {
		pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
		return NULL;
	}	
	
	/* Check if Signature is set correct */
	if(bpb->Signature != SLIM32_SIGNATURE) {
		pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
		return NULL;
	}
	
	/* Save usable constants */
	slim32->fat_begin_lba			= *(uint16_t *)(addr + 0x0E);
	slim32->cluster_begin_lba		= *(uint16_t *)(addr + 0x0E) +
										(*(uint8_t *)(addr + 0x10) * *(uint32_t *)(addr + 0x24));
	slim32->sectors_per_cluster		= *(uint8_t *)(addr + 0x0D);
	slim32->root_dir_first_cluster	= *(uint32_t *)(addr + 0x2C);
	
	/* Set up pointers to all functions for vfs to use */
	fs->unmount	= slim32_unmount;
	fs->open	= slim32_open;
	fs->close	= slim32_close;
	fs->create	= slim32_create;
	fs->remove	= slim32_remove;
	fs->read	= slim32_read;
	fs->write	= slim32_write;
	fs->getfree	= slim32_getfree;
	
	return fs;
}

int slim32_unmount(fs_t *fs) {
	return 0;
}
int slim32_open(fs_t *fs, char *filename) {
	return 0;
}
int slim32_close(fs_t *fs, int fileid) {
	return 0;
}
int slim32_create(fs_t *fs, char *filename, int size) {
	return 0;
}
int slim32_remove(fs_t *fs, char *filename) {
	return 0;
}
int slim32_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset) {
	return 0;
}
int slim32_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset) {
	return 0;
}
int slim32_getfree(fs_t *fs) {
	return 0;
}
 
 /** @} */
