#include <linux/types.h>
#include <linux/fs>
#include <libdevmapper.h>
#include <sys/stat.h>
#include <limits.h>
#include "params.h"
#include "mapper.h"
#include "util.h"
#include "loop.h"

#define T device_set_t

struct T
{
	int data_loop_size;
	int metadata_loop_size;
	int thinpool_block_size;
	char filesystem[16];
	char device_prefix[PATH_MAX];
	char root_path[PATH_MAX];
	char metadata_path[PATH_MAX];
	char data_loop_path[PATH_MAX];
	char metadata_loop_path[PATH_MAX];
};

int create_devmapper_device(char *path, int size, loop_dev_t *dev)
{
	int fd, ret;
	if ((fd = create_sparse_file(path, size)) < 0)
	{
		ret = -1;
		goto err_create_file;
	}
	
	if ((*dev = new_loop_device()) == NULL)
	{	
		ret = -1;
		goto err_new_loop;
	}

	if ((ret = attach_loop_device(fd, *dev) < 0))
		goto err_attach_dev;
	return fd;

err_attach_dev:
	free_loop_device(dev);
err_new_loop:
	close(fd);
	unlink(path);
err_create_file:
	return ret;
}

void destory_devmapper_device(char *path, int fd, loop_dev_t *dev)
{
	detach_loop_device(fd, *dev);
	free_loop_device(dev);
	close(fd);
	unlink(path);
}

int create_thinpool(char *name, loop_dev_t data_dev, loop_dev_t metadata_dev, int block_size)
{
	int ret, size, cookie;
	struct dm_task *task;
	char params[128];
	if ((ret = ioctl(data_dev, BLKGETSIZE64, &size)) < 0)
		goto err_ioctl;
	
	if ((task = dm_task_create(DM_DEVICE_CREATE)) == NULL)
	{
		ret = -1;
		goto err_create;
	}

	if ((ret = dm_task_set_name(task, name)) != 1)
	{
		ret = -1;
		goto err_set_name;
	}
	snprintf(params, "%s %s %d 32768 1 skip_block_zeroing", get_loop_device_path(metadata_dev), 
		get_loop_device_path(data_dev), device_set->thinpool_block_size);
	if ((ret = dm_task_add_target(task, 0, size/512, "thin-pool", params)) != 1)
	{
		ret = -1;
		goto err_add_target;
	}
	if ((ret = dm_task_set_cookie(task, &cookie, 0)) != 1)
	{
		ret = -1;
		goto err_set_cookie;
	}
	if ((ret = dm_task_run(task)) != 1)
	{
		ret = -1;
		goto err_task_run;
	}

	if ((ret = dm_udev_wait(cookie)) != 1)
	{
		ret = -1;
		goto err_udev_wait;
	}
	return 0;
err_udev_wait:
err_task_run:
err_set_cookie:
err_add_target:
err_set_name:
	dm_task_destory(task);
err_create:
err_ioctl:
return ret;

}

char *thinpool_name(T device_set)
{
	static char thinpool_name[PATH_MAX];
	int len = strlen(device_set->device_prefix);
	char *tmp = thinpool_name + len;

	strcpy(thinpool_name, device_set->device_prefix);
	strcpy(tmp, "-pool");
	return thinpool_name;
}

int init_device_mapper(T device_set)
{
	int ret;
	struct stat buf;
	int data_loop_fd, metadata_loop_fd;
	loop_dev_t data_loop_dev, metadata_loop_dev;
	char thinpool_name[PATH_MAX];

	if ((ret=mkdir(DEFAULT_METADATA_PATH, S_IRWXU|S_IRWXG|S_IRWXO)) < 0)
		goto err;
	if ((ret = stat(device_set->root_path, &buf)) < 0)
		goto err_stat;		
	snprintf(device_set->device_prefix, "packer-%d:%d-%d", MAJOR(buf.st_dev), 
		MINOR(buf.st_dev), buf.st_ino);

	ret = -1;
	if ((data_loop_fd = create_devmapper_device(device_set->data_loop_path, 
		device_set->data_loop_size, &data_loop_dev)) < 0)
		goto err_create_data;
	if ((metadata_loop_fd = create_devmapper_device(device_set->metadata_loop_path, 
		device_set->data_loop_size, &metadata_loop_dev)) < 0)
		goto err_create_metadata;

	snprintf(thinpool_name, dev)
	if ((ret = create_thinpool(thinpool_name(device_set->device_prefix), data_loop_dev,
	 metadata_loop_dev, device_set->thinpool_block_size)) < 0)
		goto err_create_pool;

	return ret;

err_create_pool:
	destory_devmapper_device(device_set->metadata_loop_path, metadata_loop_fd, &data_loop_dev);
err_create_metadata:
	destory_devmapper_device(device_set->data_loop_path, data_loop_fd, &data_loop_dev);
err_create_data:
err_stat:
	rmdir(metadata_dir(device_set));
err:
return ret;

}

T new_device_set(char *root)
{
	int ret;
	T device_set;

	NEW0(device_set);
	device_set->data_loop_size 		= DEFAULT_DATA_LOOP_SIZE;
	device_set->metadata_loop_size 	= DEFAULT_METADATA_LOOP_SIZE;
	device_set->thinpool_block_size = DEFAULT_THINPOOL_BLOCK_SIZE;
	strcpy(device_set->filesystem,	DEFAULT_FILESYSTEM);
	strcpy(device_set->root_path, 	DEFAULT_ROOT_PATH);
	strcpy(device_set->metadata_path, 	DEFAULT_METADATA_PATH);
	strcpy(device_set->data_loop_path, 	DEFAULT_DATA_LOOP_PATH);
	strcpy(device_set->metadata_loop_path, DEFAULT_METADATA_LOOP_PATH);
	
	if ((ret = init_device_mapper(device_set)) < 0)
		FREE(&device_set);
	return device_set;
}




