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
uint16_t read_16(uint8_t *addr) {
	uint8_t b0 = *(uint8_t *)(addr);
	uint8_t b1 = *(uint8_t *)(addr+1);
	return b0 | b1 << 8;
}

uint32_t read_32(uint8_t *addr) {
	uint8_t b0 = *(uint8_t *)(addr);
	uint8_t b1 = *(uint8_t *)(addr+1);
	uint8_t b2 = *(uint8_t *)(addr+2);
	uint8_t b3 = *(uint8_t *)(addr+3);
	return b0 | b1 << 8 | b2 << 16 | b3 << 24;
}

void remove_space_padding(char *string) {
	int i;
	for(i = 0; string[i] != '\0'; i++) {
		if (string[i] == ' ') {
			string[i] = '\0';
		}
	}
}

/* FAT32 layout information */
typedef struct {
	/* All information from Volume ID */
	uint16_t	BytsPerSec;
	uint8_t		SecPerClus;
	uint16_t	RsvdSecCnt;
	uint8_t		NumFATs;
	uint32_t	FATSz32;
	uint32_t	RootClus;
	uint16_t	Signature;
	
	/* "Usable" information */
	uint16_t	fat_begin_lba;
	uint32_t	cluster_begin_lba;
	uint32_t	sectors_per_cluster;
	uint32_t	root_dir_first_cluster;
} slim32_t;

fs_t * slim32_init(gbd_t *disk) {
	uint32_t addr;
	fs_t *fs;
	gbd_request_t req;
	slim32_t *slim32;
	int r;
	
	/* Local copies of the Volume ID information */
	char name[SLIM32_VOLUMELABEL_SIZE];
	uint16_t	BytsPerSec;
	uint8_t		SecPerClus;
	uint16_t	RsvdSecCnt;
	uint8_t		NumFATs;
	uint32_t	FATSz32;
	uint32_t	RootClus;
	uint16_t	Signature;
	
	uint16_t	fat_begin_lba;
	uint32_t	cluster_begin_lba;
	uint32_t	sectors_per_cluster;
	uint32_t	root_dir_first_cluster;
	
	addr = pagepool_get_phys_page();
	if(addr == 0) {
		kprintf("slim32_init: could not allocate memory.\n");
		return NULL;
	}
	addr = ADDR_PHYS_TO_KERNEL(addr);	// transform to vm address
	
    /* Assert that one page is enough */
    KERNEL_ASSERT(PAGE_SIZE >= (sizeof(slim32_t) + sizeof(fs_t)));
	
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
	
	/* Copy disk information */
	stringcopy(name, (char *)(addr + 0x47), SLIM32_VOLUMELABEL_SIZE);
	remove_space_padding(name);
	
	BytsPerSec	= read_16((uint8_t *)(addr + 0x0B));
	SecPerClus	= *(uint8_t *)(addr + 0x0D);
	RsvdSecCnt	= read_16((uint8_t *)(addr + 0x0E));
	NumFATs		= *(uint8_t *)(addr + 0x10);
	FATSz32		= read_32((uint8_t *)(addr + 0x24));
	RootClus	= read_32((uint8_t *)(addr + 0x2C));
	Signature	= read_16((uint8_t *)(addr + 0x1FE));
	/* Check if Bytes Per Sector is set correct */
	if(BytsPerSec != SLIM32_BYTS_PER_SEC) {
		pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
		return NULL;
	}

	/* Check if Number of FATs is set correct */
	if(NumFATs != SLIM32_NUMBER_OF_FATS) {
		pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
		return NULL;
	}	
	
	/* Check if Signature is set correct */
	if(Signature != SLIM32_SIGNATURE) {
		pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
		return NULL;
	}
	
	/* Save usable constants */
	fat_begin_lba			= RsvdSecCnt;
	cluster_begin_lba		= RsvdSecCnt + (NumFATs * FATSz32);
	sectors_per_cluster		= SecPerClus;
	root_dir_first_cluster	= RootClus;
	
	/* Save variables in structure */
	fs	= (fs_t *)addr;
	slim32 = (slim32_t *)(addr + sizeof(fs_t));
	
	slim32->BytsPerSec	= BytsPerSec;
	slim32->SecPerClus	= SecPerClus;
	slim32->RsvdSecCnt	= RsvdSecCnt;
	slim32->NumFATs		= NumFATs;
	slim32->FATSz32		= FATSz32;
	slim32->RootClus	= RootClus;
	slim32->Signature	= Signature;
	
	/* Save usable constants */
	slim32->fat_begin_lba			= fat_begin_lba;
	slim32->cluster_begin_lba		= cluster_begin_lba;
	slim32->sectors_per_cluster		= sectors_per_cluster;
	slim32->root_dir_first_cluster	= root_dir_first_cluster;
	
	stringcopy(fs->volume_name, name, VFS_NAME_LENGTH);
	
	fs->internal = (void *)slim32;
	
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
	/* Free the physical memory given in the init function */
	pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS((uint32_t)fs));
	return VFS_OK;
}
int slim32_open(fs_t *fs, char *filename) {
	slim32_t *slim32;
	slim32 = (slim32_t *)fs->internal;
	
	kprintf("Trying to open: %s\n", filename);
	/* How to calculate address of a cluster:
	uint32_t lba_addr = slim32->cluster_begin_lba + (cluster_number - 2) * slim32->sectors_per_cluster;
	*/
	/*
		uint8_t *first_byte;
	first_byte = (uint8_t *)(slim32->cluster_begin_lba);
	int x = 0;
	kprintf("1\nCluster: %i\n", first_byte);
	while(*first_byte != 0) {
		kprintf("Record: %i\n", x);
		// Set first byte of the record
		first_byte = &((uint8_t *)(slim32->cluster_begin_lba))[SLIM32_RECORD_SIZE*x];
		
		// Check if record is in use 
		if (*first_byte != 0xE5) {
			unsigned int attrib_volumeID = *((&(first_byte[11]))+3);
			if (attrib_volumeID == 1) {
				// The current record is the Volume ID; save the filename as fs->volume_name 
				stringcopy(fs->volume_name, (char *)first_byte, SLIM32_SHORT_FILENAME_SIZE);
				break;
			}
		}
		++x;
	}*/
	fs = fs;
	filename = filename;
	return (int)(slim32->cluster_begin_lba);
}
int slim32_close(fs_t *fs, int fileid) {
	fs = fs;
	fileid = fileid;
	return 0;
}
int slim32_create(fs_t *fs, char *filename, int size) {
	fs = fs;
	filename = filename;
	size = size;
	return 0;
}
int slim32_remove(fs_t *fs, char *filename) {
	fs = fs;
	filename = filename;
	return 0;
}
int slim32_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset) {
	fs = fs;
	fileid = fileid;
	buffer = buffer;
	bufsize = bufsize;
	offset = offset;
	return 0;
}
int slim32_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset) {
	fs = fs;
	fileid = fileid;
	buffer = buffer;
	datasize = datasize;
	offset = offset;
	return 0;
}
int slim32_getfree(fs_t *fs) {
	fs = fs;
	return 0;
}
 
 /** @} */
