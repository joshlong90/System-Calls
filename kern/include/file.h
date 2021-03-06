/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>

/*
 * Put your function declarations and data types here ...
 */

/* used as integer entry value for slot that is free in process file descriptor table. */
#define FREE_SLOT       -1
/* stdout and stderr slot locations in global open file table. */
#define GLOBAL_STDOUT   0
#define GLOBAL_STDERR   1
/* stdout and stderr file descriptors in per-process file table. */
#define STDOUT_FD       1
#define STDERR_FD       2

/* global open file table entry includes file pointer (offset) and vnode pointer. */
struct of_entry {
    off_t fp;
    struct vnode *v_ptr;
    int ref_count;
};

/* global open file table data structure, lock is included for synchronisation. */
struct of_table {
    struct of_entry *open_files[OPEN_MAX];
    struct lock *oft_lock;
};


/* 
 * system calls
 */
/* sys_open - open a file and return file descriptor, -1 if error. */
int sys_open(const_userptr_t filename, int flags, mode_t mode, int *fd);

/* sys_close - close a file and return 0, -1 if error. */
int sys_close(int fd, int *retval);

/* sys_write - write out to a file located at fd. */
int sys_write(int fd, userptr_t buf, size_t nbytes, int *bytes_written);

/* sys_read - read from a file located at fd. */
int sys_read(int fd, userptr_t buf, size_t nbytes, int *bytes_read);

/* sys_lseek - alters seek location of file, to a new position based on pos and whence. */
int sys_lseek(int fd, off_t offset, int whence, off_t *new_fp);

/* sys_dup2 - clones the file handle oldfd onto the file handle newfd. */
int sys_dup2(int old_fd, int new_fd, int *retval);


/*
 * global open file table
 */
/* initialises global open file table, used during boot() of os161. */
int init_global_oft(void);

/* adds an entry into global oft with vnode pointer and file pointer (offset). */
int add_global_oft(off_t fp, struct vnode *v_ptr, int *ofptr);

/* removes an entry from the global oft. */
int rem_global_oft(int ofptr);

/* update the file pointer for the entry at location ofptr. */
int upd_global_oft(int ofptr, off_t offset, int whence, off_t *new_fp);

/* free the global oft in case there is an issue during initialisation. */
void free_global_oft(struct of_table *global_oft);

/* print the global oft contents for debugging. */
void print_global_oft(struct of_table *global_oft);


/* per-process file tables and all associated functions are implemented in proc.c. */

#endif /* _FILE_H_ */
