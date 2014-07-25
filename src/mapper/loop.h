#ifndef _MY_LOOP_H
#define _MY_LOOP_H

#define T loop_dev_t
typedef struct T *T;

extern T new_loop_device(void);
extern void free_loop_device(T *dev);

extern int attach_loop_device(int fd, T dev);
extern void detach_loop_device(int fd, T dev);

extern int get_loop_device_fd(T dev);
extern int get_loop_device_path(T dev);

#undef T
#endif
