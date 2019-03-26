#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>

/*
 * Add your file-related functions here ...
 */

/* global open file table */
struct of_table *global_oft;

/*
NOTES
uiou_init helper function can encapsulate the initialisation of uio instances. 2018 ext lecture ASST2 walkthrough

sys_open() --> vfs_open(), copyinstr()
sys_close() --> vfs_close()
sys_read() --> VOP_READ()
sys_write() --> VOP_WRITE()
sys_lseek() --> VOP_ISSEEKABLE(), VOP_STAT()
sys_dup2() --> doesn't need to call anything else

lseek() Offset is 64-bit and resides in 2 32-bit registers.
join32to64(tf->tf_a2, tf->tf_a3, &offset); // need to collect from 2 registers
copyin((userptr_t)tf->tf_sp+16, &whence, sizeof(int)); // 3rd argument needs to be fetched from stack
split64to32(retval64, &tf->tf_v0, &tf->tf_v1); // return the offset
*/

/*
 * opens a file with specified path name, returns file descriptor if successful, -1 otherwise.
 */
int sys_open(const_userptr_t filename, int flags, mode_t mode, int *fd_ptr) {
    struct vnode *v_ptr = NULL;     // return vnode from vfs_open()
    char sys_filename[PATH_MAX];    // kernel address copy location for filename
    size_t path_len = 0;            // initialised to 0 and measured in copyinstr()
    off_t fp = 0;                   // open all files with offset of 0
	int err, ofptr;

    /* initialise the file descriptor to an invalid value */
    *fd_ptr = -1;                       

    /* copy the file name from user address to kernel address - as the user cannot be trusted. */
    err = copyinstr(filename, sys_filename, PATH_MAX, &path_len);
    if (err) {
        return err;
    }

    /* use virtual file system open operation and retrieve vnode pointer. */
    err = vfs_open(sys_filename, flags, mode, &v_ptr);
    if (err) {
        return err;
    }

    /* place the vnode pointer and file pointer into the global open file table. */
    err = add_global_oft(fp, v_ptr, &ofptr);
    if (err) {
        return err;
    }

    /* place the open file pointer into the per process file table and save file descriptor. */
    err = proc_addfd(ofptr, fd_ptr);
    if (err) {
        return err;
    }

    return 0;
}

/*
 * closes the file specified by fd, returns 0 if successful, -1 otherwise.
 */
int sys_close(int fd, int *retval) {
    struct vnode *v_ptr = NULL;
    int ofptr;
    *retval = -1;

    /* check that file descriptor is in valid range */
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    /* retrieve open file pointer from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr == FREE_SLOT) {
        return EBADF;
    }
    /* retrieve vnode pointer from global open file table. */
    v_ptr = global_oft->open_files[ofptr]->v_ptr;

    /* use virtual file system close operation and close vnode. */
    vfs_close(v_ptr);

    /* free open file entry in global open file table. */
    // may need to consider refcount in global file table with dup2
    rem_global_oft(ofptr);

    /* free file descriptor in process open file table. */
    proc_remfd(fd);
    *retval = 0;
    return 0;
}

/*
 * writes to file specified by fd at the location of the current file pointer.
 */
int sys_write(int fd, userptr_t buf, size_t nbytes, int *bytes_written) {
    struct vnode *v_ptr = NULL;
    struct uio myuio;
    struct iovec iov;
    enum uio_rw rw = UIO_WRITE;
    off_t fp;
    int ofptr, err;

    /* initialise the bytes written to an invalid value */
    *bytes_written = -1;

    /* check that file descriptor is in valid range */
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    /* retrieve open file ptr from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr == FREE_SLOT) {
        return EBADF;
    }
    /* retrieve vnode pointer and file pointer from global open file table. */
    v_ptr = global_oft->open_files[ofptr]->v_ptr;
    fp = global_oft->open_files[ofptr]->fp;

    /* initialise uio structure for writing to file */
    uio_kinit(&iov, &myuio, buf, nbytes, fp, rw);

    /* write to file. */
    err = VOP_WRITE(v_ptr, &myuio);
    if (err) {
        return err;
    }

    /* record the amount of bytes written to file. */
    *bytes_written = (int) nbytes - (int) myuio.uio_resid;

    /* update file pointer to new location in global open file table. */
    upd_global_oft(ofptr, bytes_written);

    return 0;
}

/*
 * reads to file specified by fd at the location of the current file pointer.
 */
int sys_read(int fd, userptr_t buf, size_t nbytes, int *bytes_read) {
    struct vnode *v_ptr = NULL;
    struct uio myuio;
    struct iovec iov;
    enum uio_rw rw = UIO_READ;
    off_t fp;
    int ofptr, err;

    /* initialise the bytes read to an invalid value */
    *bytes_read = -1;

    /* check that file descriptor is in valid range */
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }
    /* retrieve open file ptr from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr == FREE_SLOT) {
        return EBADF;
    }
    /* retrieve vnode pointer and file pointer from global open file table. */
    v_ptr = global_oft->open_files[ofptr]->v_ptr;
    fp = global_oft->open_files[ofptr]->fp;

    /* initialise uio structure for writing to file */
    uio_kinit(&iov, &myuio, buf, nbytes, fp, rw);

    /* read file. */
    err = VOP_READ(v_ptr, &myuio);
    if (err) {
        return err;
    }

    /* record the amount of bytes read from file. */
    *bytes_read = (int) nbytes - (int) myuio.uio_resid;

    /* update file pointer to new location in global open file table. */
    upd_global_oft(ofptr, bytes_read);

    return 0;
}

/*
 *  alters the current seek position, seeking to new position based on pos and whence
 */
int sys_lseek(int fd, off_t pos, int whence, off_t *new_position)
{
    int ofptr;//, err;
    /* check that file descriptor is in valid range */
    if (fd < 0 || fd >= OPEN_MAX) {
        return EBADF;
    }

    /* retrieve open file ptr from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr == FREE_SLOT) {
        return EBADF;
    }

    global_oft->open_files[ofptr];

    /* initialise new position to an invalid value */
    *new_position = -1;

    switch (whence)
    {
        case SEEK_SET:
            /* new position is pos */
            *new_position = pos;
            break;
    
        case SEEK_CUR:
            /* new position is current position + pos */
            break;
    
        case SEEK_END:
            /* new position is end-of-file + pos */
            break;
    
        default:
            /* whence is invalid */
            return EINVAL;
    }
    /* TODO: update the new position */
    return 0;
}

/* 
 * initialise the global open file table, completed during boot() "main.c".
 * attach the stdout and stderr open files connected to "con:".
 */
int init_global_oft(void) {
    char stdout[] = "con:";
    char stderr[] = "con:";
    struct vnode *v_out = NULL;
    struct vnode *v_err = NULL;
    int err, i, ofptr;

    /* allocate memory for global open file table */
    global_oft = (struct of_table *) kmalloc(sizeof(struct of_table));
    if (global_oft == NULL) {
        panic("Cannot allocate memory for global open file table.");
        return ENOMEM;
    }
    /* initialise all global open file entries to empty slots (NULL). */
    for (i = 0; i < OPEN_MAX; i++) global_oft->open_files[i] = NULL;

    /* allocate memory for global open file table lock */
    global_oft->oft_lock = lock_create("global oft lock");
    if (global_oft->oft_lock == NULL) {
        kfree(global_oft);
        panic("Cannot allocate memory for global open file table lock.");
        return ENOMEM;
    }

    /* stdout */
    /* initialise stdout and connect to console ("con:"). */
    err = vfs_open(stdout, O_WRONLY, 0, &v_out);
    if (err) {
        vfs_close(v_out);
        free_global_oft(global_oft);
        panic("Could not connect stdout to console.");
        return err; 
    }
    /* place the stdout open file in the global open file table. */
    err = add_global_oft(0, v_out, &ofptr);
    if (err) {
        free_global_oft(global_oft);
        panic("Could not add stdout to global open file table.");
        return err;
    }

    /* stderr */
    /* initialise stderr vnode and connect to console ("con:"). */
    err = vfs_open(stderr, O_WRONLY, 0, &v_err);
    if (err) {
        vfs_close(v_err);
        free_global_oft(global_oft);
        panic("Could not connect stderr to console.");
        return err;
    }
    /* place the stderr open file in the global open file table. */
    err = add_global_oft(0, v_err, &ofptr);
    if (err) {
        free_global_oft(global_oft);
        panic("Could not add stderr to global open file table.");
        return err;
    }

    return 0;
}

/* 
 * adds a new open file to the first available slot in the global open file table.
 */
int add_global_oft(off_t fp, struct vnode *v_ptr, int *ofptr) {
    int i;
    struct of_entry *new_file;

    KASSERT(fp >= 0);

    new_file = (struct of_entry *) kmalloc(sizeof(struct of_entry));
    if (new_file == NULL) {
        return ENOMEM;
    }
    new_file->fp = fp;
    new_file->v_ptr = v_ptr;
    lock_acquire(global_oft->oft_lock);
    for (i = 0; i < OPEN_MAX; i++) {
        if (global_oft->open_files[i] == NULL) {
            global_oft->open_files[i] = new_file;
            lock_release(global_oft->oft_lock);
            *ofptr = i;
            return 0;
        }
    }
    lock_release(global_oft->oft_lock);
    kfree(new_file);
    return ENFILE;
}

/*
 * free the open file at the specified ofptr location and set to free slot (NULL).
 */
int rem_global_oft(int ofptr) {
    KASSERT(ofptr >= 0 && ofptr < OPEN_MAX);

    lock_acquire(global_oft->oft_lock);
    kfree(global_oft->open_files[ofptr]);
    global_oft->open_files[ofptr] = NULL;
    lock_release(global_oft->oft_lock);

    return 0;
}

/*
 * update file pointer at entry ofptr.
 */
int upd_global_oft(int ofptr, int *bytes_written) {
    off_t new_fp;
    lock_acquire(global_oft->oft_lock);
    new_fp = global_oft->open_files[ofptr]->fp + (off_t) *bytes_written;
    global_oft->open_files[ofptr]->fp = new_fp;
    lock_release(global_oft->oft_lock);
    return 0;
}

/*
 * free all allocated memory associated with the global open file table.
 */
void free_global_oft(struct of_table *global_oft) {
    int i;
    
    for (i = 0; i < OPEN_MAX; i++) {
        if (global_oft->open_files[i] != NULL) {
            kfree(global_oft->open_files[i]);
        }
    }

    lock_destroy(global_oft->oft_lock);
    kfree(global_oft);
    return;
}

/*
 * prints all entries present in the global open file table.
 */
void print_global_oft(struct of_table *global_oft) {

    KASSERT(global_oft != NULL);

    int i;
    for (i = 0; i < OPEN_MAX; i++) {
        if (global_oft->open_files[i] != NULL) {
            kprintf("\nOpen file at global oft location %d\n", i);
            kprintf("\tFile pointer offset = %d\n", (int)global_oft->open_files[i]->fp);
            kprintf("\tVnode reference count = %d\n", global_oft->open_files[i]->v_ptr->vn_refcount);
        }
    }
}
