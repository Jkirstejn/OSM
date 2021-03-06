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
	
	gbd_t 		*disk;
	uint32_t	*buffer;
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
    KERNEL_ASSERT(PAGE_SIZE >= sizeof(SLIM32_BYTS_PER_SEC) + (sizeof(slim32_t) + sizeof(fs_t)));
	
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
	
	kprintf("%i\n", BytsPerSec);
	kprintf("%i\n", SecPerClus);
	kprintf("%i\n", RsvdSecCnt);
	kprintf("%i\n", FATSz32);
	kprintf("%i\n", NumFATs);
	
	
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
	slim32->disk = disk;
	slim32->buffer = (uint32_t *)(addr + sizeof(fs_t) + sizeof(slim32_t));
	kprintf("Buffer: %p\n", slim32->buffer);
	
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

int record_is_volume_id(uint8_t *attrib) {
	int test = (SLIM32_ATTR_VOLUME_BITMASK & *attrib);
	kprintf("Bitmaske: %i\nAttrib: %i\ntest: %i\n", SLIM32_ATTR_VOLUME_BITMASK, *attrib, test);
	if (test == SLIM32_ATTR_VOLUME_BITMASK) {
		return 0;
	}
	return 1;
}

int search_cluster(fs_t *fs, uint32_t *sector_addr, char *filename) {
	slim32_t *slim32;
	slim32 = (slim32_t *)fs->internal;
	
	uint32_t *offset = sector_addr;
	unsigned int i;
	
	/* Calculate number of records before we have to find the next cluster */
	unsigned int records_per_cluster = slim32->BytsPerSec / SLIM32_RECORD_SIZE * slim32->SecPerClus;
	kprintf("Offset: %i\n", *offset);
	/* Check the given cluster for the file */
	for(i = 0; (*(uint8_t *)(offset) != 0) && (records_per_cluster > i); i++) {
		kprintf("I: %i\n", i);
		offset = (uint32_t *)((sector_addr) + (SLIM32_RECORD_SIZE*i));
		/* Check if entry is used */
		char *name = "";
		stringcopy(name, (char *)offset, SLIM32_SHORT_FILENAME_SIZE);
		kprintf("Navn: [%s]\n", name);
		if (*(uint8_t *)(offset) != 0xE5) {
			//uint8_t *attrib_byte = (uint8_t *)(&offset[SLIM32_SHORT_FILENAME_SIZE]);
			/* Perform comparison between record and desired filename */
			int x;
			for(x = 0; x < 32; x++) {
				kprintf("Byte:\t%i\tIndhold:\t%i\n", x, *(uint8_t *)(offset + x));
			}
			kprintf("End of record\n");
			if ((stringcmp(filename, (char *)offset) == 0) ||
				(record_is_volume_id((uint8_t *)(offset + SLIM32_SHORT_FILENAME_SIZE + 5)) == 0)) {
				return *offset;
			}
		}
	}
	if (offset[0] == 0) {
		kprintf("End of directory\n");
		return VFS_NOT_FOUND;
	} else {
		return VFS_NOT_FOUND;
		//return search_sector();
	}
}

int search_for_file(fs_t *fs, char *filename) {
	slim32_t *slim32;
	slim32 = (slim32_t *)fs->internal;
	gbd_request_t req;
	int r;
	
	/* Read root block from disk */
	uint32_t lba_addr = slim32->cluster_begin_lba + (slim32->root_dir_first_cluster - 2) * slim32->sectors_per_cluster;
	kprintf("LBA: %i\n", lba_addr);
	req.block	= lba_addr;
	req.buf		= ADDR_KERNEL_TO_PHYS((uint32_t) slim32->buffer);
	req.sem		= NULL;
	r = slim32->disk->read_block(slim32->disk, &req);
	if (r == 0) {
		kprintf("Fejl for helvede! Din NOOB!\n");
		return VFS_ERROR;
	}
	return search_cluster(fs, slim32->buffer, filename);
}

int slim32_open(fs_t *fs, char *filename) {
	slim32_t *slim32;
	//gbd_request_t req;
	
	slim32 = (slim32_t *)fs->internal;
		
	kprintf("Trying to open: %s\n", filename);
	int rtn = search_for_file(fs, filename);
	kprintf("%i", rtn);
	return rtn;
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
