#include <unistd.h>
#include <fcntl.h>
#include "util.h"

int create_sparse_file(char *path, int size)
{
	int ret, fd;
	if ((ret = open(path, O_RDWR|O_CREAT)) < 0)
		goto err;
	fd = ret;
	if ((ret = lseek(fd, size, SEEK_SET)) == -1)
		goto err_seek;
	return fd;

err_seek:
	close(fd);	
	unlink(path);
err:
	return ret;
}