#include <linux/loop.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "params.h"
#include "loop.h"

#define T loop_dev_t

struct T
{
	int loop_fd;
	int loop_index;
};

int open_loop_device(int fd, T dev)
{
	int ret, index, loop_fd;
	char target[LO_NAME_SIZE];
	struct stat buf;

	index = -1;
	for(;;)
	{
		index++;
		snprintf(target, LO_NAME_SIZE, "/dev/loop%d", index);
		
		if ((ret = stat(target, &buf)) < 0)
			break;
		
		if (!S_ISBLK(buf.st_mode))
			continue;
		
		if ((ret = open(target, O_RDWR)) < 0)
			break;
		loop_fd = ret;
		
		if ((ret = ioctl(loop_fd, LOOP_SET_FD, fd)) < 0)
			close(loop_fd);

		dev->loop_fd = loop_fd;
		dev->loop_index = index;
		break;
	}
	return ret;
}

void close_loop_device(int fd, T dev)
{
	ioctl(dev->loop_fd, LOOP_CLR_FD, fd);
	close(dev->loop_fd);
	return;
}

int attach_loop_device(int fd, T dev)
{
	int ret, loop_fd;
	struct loop_info64 loop_info;

	if ((ret= open_loop_device(fd, dev)) < 0)
		return ret;

	loop_info.lo_offset = 0;
	loop_info.lo_flags = LO_FLAGS_AUTOCLEAR;
	snprintf(loop_info.lo_name, LO_NAME_SIZE, "/dev/loop%d", loop_index);

	if ((ret = ioctl(dev->loop_fd, LOOP_SET_STATUS64, &loop_info)) < 0)
		close_loop_device(fd, dev);
	return ret;
}

void detach_loop_device(int fd, T dev)
{
	struct loop_info64 loop_info;
	
	loop_info.lo_offset = 0;
	loop_info.lo_flags = 0;
	snprintf(loop_info.lo_name, LO_NAME_SIZE, "/dev/loop%d", loop_index);
	ioctl(dev->loop_fd, LOOP_SET_STATUS64, &loop_info);

	close_loop_device(fd, dev);
	return;
}

T new_loop_device(void)
{
	T dev;
	NEW0(dev);
	return dev;
}
void free_loop_device(T *dev)
{
	FREE(*dev);
}

int get_loop_device_fd(T dev)
{
	return dev->loop_fd;
}

char get_loop_device_path(T dev)
{
	static char path[PATH_MAX];
	snprintf(path, "/dev/loop%d", dev->loop_index);
	return path;
}
