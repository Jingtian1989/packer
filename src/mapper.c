#include <linux/types.h>
#include <linux/fs>
#include <libdevmapper.h>
#include <sys/stat.h>
#include <limits.h>
#include "params.h"
#include "mapper.h"
#include "util.h"
#include "loop.h"
#include "config.h"
#include "table.h"

#define T device_set_t

struct T
{
	int next_device_id;

	struct device_info *base_device;

	int data_loop_size;
	int metadata_loop_size;
	int thinpool_block_size;

	char *filesystem;
	char *root_path;
	char *metadata_path;
	char *data_loop_path;
	char *metadata_loop_path;

	char device_prefix[PATH_MAX];

};

struct device_info
{
	int device_id;
	T device_set;
};

static int devmapper_busy;
static int devmapper_exist;

static void devmapper_log_callback(int level, char *file, int line, int dm_errno_or_class, char *str)
{
	if (level < 7)
	{
		if (strstr(str, "busy") != NULL)
			devmapper_busy = true;
		if (strstr(str, "File exists") != NULL)
			devmapper_exist = true;
	}
}

static void log_cb(int level, const char *file, int line, int dm_errno_or_class, const char *f, ...)
{
	char buffer[256];
	va_list ap;

	va_start(ap ,f);
	vsnprintf(buffer, 256, f, ap);
	va_end(ap);

	devmapper_log_callback(level, (char *)file, line, dm_errno_or_class, buffer);
}

static char *thinpool_name(T device_set)
{
	static char thinpool_name[PATH_MAX];
	int len = strlen(device_set->device_prefix);
	char *tmp = thinpool_name + len;

	strcpy(thinpool_name, device_set->device_prefix);
	strcpy(tmp, "-pool");
	return thinpool_name;
}

static int create_devmapper_device(char *path, int size, loop_dev_t *dev)
{
	int fd, ret;

	ret = -1;
	if ((fd = create_sparse_file(path, size)) < 0)
		goto err_create_file;
	
	if ((*dev = new_loop_device()) == NULL)
		goto err_new_loop;

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

static void destory_devmapper_device(char *path, int fd, loop_dev_t *dev)
{
	detach_loop_device(fd, *dev);
	free_loop_device(dev);
	close(fd);
	unlink(path);
}

static int create_thinpool(char *name, loop_dev_t data_dev, loop_dev_t metadata_dev, int block_size)
{
	int ret, size, cookie;
	struct dm_task *task;
	char params[128];
	if ((ret = ioctl(data_dev, BLKGETSIZE64, &size)) < 0)
		goto err_ioctl;
	
	ret = -1;
	if ((task = dm_task_create(DM_DEVICE_CREATE)) == NULL)
		goto err_create;

	if ((ret = dm_task_set_name(task, name)) != 1)
		goto err_set_name;

	snprintf(params, "%s %s %d 32768 1 skip_block_zeroing", get_loop_device_path(metadata_dev), 
		get_loop_device_path(data_dev), device_set->thinpool_block_size);

	if ((ret = dm_task_add_target(task, 0, size/512, "thin-pool", params)) != 1)
		goto err_add_target;

	if ((ret = dm_task_set_cookie(task, &cookie, 0)) != 1)
		goto err_set_cookie;
		
	if ((ret = dm_task_run(task)) != 1)
		goto err_task_run;

	if ((ret = dm_udev_wait(cookie)) != 1)
		goto err_udev_wait;
	return 0;

err_udev_wait:
err_task_run:
err_set_cookie:
err_add_target:
err_set_name:
	ret = -1;
	dm_task_destory(task);
err_create:
err_ioctl:
return ret;

}

static int create_device(struct device_info *info, int *id)
{
	int ret;
	struct dm_task *task;
	char params[256];

	task = dm_task_create(DM_DEVICE_TARGET_MSG, thinpool_name(info->device_set->device_prefix));
	if (task == NULL)
		return -1;

	for (;;)
	{
		if ((ret = dm_task_set_sector(task, 0)) != 1)
			goto err;
		snprintf(params, "create_thin %d", *id);
		if ((ret = dm_task_set_message(task, params)) != 1)
			goto err;
		devmapper_exist = 0;
		if ((ret = dm_task_run(task)) != 1)
		{
			if (devmapper_exist)
			{
				*id++;
				continue;
			}
			goto err;		
		}
		info->device_id = *id;
		return 0;
	}

err:
	dm_task_destory(task);
	return -1;
}

static int remove_device(char *name)
{
	int ret;
	struct dm_task *task;

	if ((task = dm_task_create(DM_DEVICE_REMOVE)) == NULL)
		goto err_create;

	if ((ret = dm_task_set_name(task, name)) != 1)
		goto err_set_name;
		
	if ((ret = dm_task_run(task)) != 1)
		goto err_task_run;

	return 0;

err_task_run:
err_set_name:
	ret = -1;
	dm_task_destory(task);
err_create:
return ret;
}

static int remove_device_wait(char *name)
{
	int ret, i;
	for (i = 0; i < 1000; i++)
	{
		if ((ret = remove_device(name)) == 0)
			break;
		sleep(1000);
	}
	return ret;
}

static int activate_device(struct device_info *info)
{
	int ret;
	struct dm_task *task;
	char params[256];
	int cookie;

	if ((task = dm_task_create(DM_DEVICE_CREATE)) == NULL)
		goto err_create;
	snprintf(params, "%s %d", thinpool_name(info->device_set), info->device_id);

	if ((ret = dm_task_add_target(task, 0, size, "thin", params)) != 1)
		goto err_add_target;

	if ((ret = dm_task_set_add_node(task, DM_ADD_NODE_ON_CREATE)) != 1)
		goto err_set_add;

	if ((ret = dm_task_set_cookie(task, &cookie, 0)) != 1)
		goto err_set_cookie;

	if ((ret = dm_task_run(task)) != 1)
		goto err_task_run;

	if ((ret = dm_udev_wait(cookie)) != 1)
		goto err_udev_wait;

	return 0;
err_udev_wait:
err_task_run:
err_set_cookie:
err_set_add:
err_add_target:
	dm_task_destory(task);
err_create:
	return -1;
}	

static int deactivate_device(struct device_info *info)
{

}


static int setup_base_image(T device_set)
{
	int ret, id = device_set->next_device_id;
	struct device_info *info;

	NEW0(info);
	info->device_set = device_set;

	if ((ret = create_device(info, &id)) < 0)
		goto err;
	device_set->next_device_id = (id + 1) & 0xffffff;

	if ((ret = activate_device(info)) < 0)
		goto err_act_dev;

	if ((ret = create_filesystem(info)) < 0)
		goto err_crt_fs;

	device_set->base_device = info;
	return ret;

err_crt_fs:
	deactive_device(info);
err_act_dev:
	delete_device(info);
err:
	FREE(&info);
	return ret;
}

static int init_device_mapper(T device_set)
{
	int ret;
	struct stat buf;
	int data_loop_fd, metadata_loop_fd;
	loop_dev_t data_loop_dev, metadata_loop_dev;
	char thinpool_name[PATH_MAX];

	if ((ret=mkdir(device_set->metadata_path, S_IRWXU|S_IRWXG|S_IRWXO)) < 0)
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

	dm_log_with_errno_init(log_cb);

	device_set->next_device_id = 0;
	if ((ret = setup_base_image(device_set)) < 0)
		goto err_setup_base;

	return ret;

err_setup_base:
	remove_device_wait(thinpool_name(device_set->device_prefix));
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
	device_set->filesystem 			= DEFAULT_FILESYSTEM;
	device_set->root_path 			= DEFAULT_ROOT_PATH;
	device_set->metadata_path 		= DEFAULT_METADATA_PATH;
	device_set->data_loop_path 		= DEFAULT_DATA_LOOP_PATH;
	device_set->metadata_loop_path 	= DEFAULT_METADATA_LOOP_PATH
	
	if ((ret = init_device_mapper(device_set)) < 0)
		FREE(&device_set);
	return device_set;
}




