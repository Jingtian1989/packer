#ifndef _MY_MAPPER_H
#define _MY_MAPPER_H


#define T device_set_t
typedef struct T *T;



#define DEFAULT_ROOT_PATH 			"/var/lib/packer/devmapper"
#define DEFAULT_METADATA_PATH 		"/var/lib/packer/devmapper/metadata"
#define DEFAULT_DATA_LOOP_PATH 		"/var/lib/packer/devmapper/devmapper/data"
#define DEFAULT_METADATA_LOOP_PATH 	"/var/lib/packer/devmapper/devmapper/metadata" 

#define DEFAULT_DATA_LOOP_SIZE 		(100 * 1024 * 1024 * 1024)
#define DEFAULT_METADATA_LOOP_SIZE 	(2 * 1024 * 1024 * 1024)
#define DEFAULT_BASE_FS_SIZE 		(10 * 1024 * 1024 * 1024)
#define DEFAULT_THINPOOL_BLOCK_SIZE (128)
#define DEFAULT_FILESYSTEM 			"ext4"

extern T new_device_set(char *root);

#undef