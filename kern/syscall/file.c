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
    ofptr = -1;                      

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
    int ofptr;

    /* initialise the return value to an invalid value */
    *retval = -1;

    /* retrieve open file pointer from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr < 0) {
        return EBADF;
    }

    /* decrement open file ref_count or free open file entry in global open file table. */
    rem_global_oft(ofptr);

    /* free file descriptor in process open file table. */
    proc_remfd(fd);
    *retval = 0;
    return 0;
}

/*
 * writes nbytes to file specified by fd at the location of the current file pointer.
 */
int sys_write(int fd, userptr_t buf, size_t nbytes, int *bytes_written) {
    struct vnode *v_ptr = NULL;
    struct uio myuio;
    struct iovec iov;
    enum uio_rw rw = UIO_WRITE;
    off_t fp, new_fp;
    int ofptr, err;

    /* initialise the bytes written to an invalid value */
    *bytes_written = -1;

    /* retrieve open file ptr from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr < 0) {
        return EBADF;
    }
    /* retrieve vnode pointer and file pointer from global open file table. */
    v_ptr = global_oft->open_files[ofptr]->v_ptr;
    fp = global_oft->open_files[ofptr]->fp;

    /* initialise uio structure for writing to file */
    uio_uinit(&iov, &myuio, buf, nbytes, fp, rw);

    /* write to file. */
    err = VOP_WRITE(v_ptr, &myuio);
    if (err) {
        return err;
    }

    /* record the amount of bytes written to file. */
    *bytes_written = (int) nbytes - (int) myuio.uio_resid;

    /* update file pointer to new location in global open file table. */
    upd_global_oft(ofptr, (off_t) *bytes_written, SEEK_CUR, &new_fp);

    return 0;
}

/*
 * reads nbytes from file specified by fd at the location of the current file pointer.
 */
int sys_read(int fd, userptr_t buf, size_t nbytes, int *bytes_read) {
    struct vnode *v_ptr = NULL;
    struct uio myuio;
    struct iovec iov;
    enum uio_rw rw = UIO_READ;
    off_t fp, new_fp;
    int ofptr, err;

    /* initialise the bytes written to an invalid value */
    *bytes_read = -1;

    /* retrieve open file ptr from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr < 0) {
        return EBADF;
    }

    /* retrieve vnode pointer and file pointer from global open file table. */
    v_ptr = global_oft->open_files[ofptr]->v_ptr;
    fp = global_oft->open_files[ofptr]->fp;

    /* initialise uio structure for reading a file */
    uio_uinit(&iov, &myuio, buf, nbytes, fp, rw);

    /* read from file. */
    err = VOP_READ(v_ptr, &myuio);
    if (err) {
        return err;
    }

    /* record the amount of bytes written to file. */
    *bytes_read = (int) nbytes - (int) myuio.uio_resid;

    /* update file pointer to new location in global open file table. */
    upd_global_oft(ofptr, (off_t) *bytes_read, SEEK_CUR, &new_fp);

    return 0;
}

/*
 * alters seek location of file, to a new position based on pos and whence.
 */
int sys_lseek(int fd, off_t offset, int whence, off_t *new_fp) {
    struct vnode *v_ptr = NULL;
    bool seekable;
    int ofptr, err;

    /* initialise the return new_fp to an invalid value */
    *new_fp = -1;

    /* retrieve open file ptr from process open file table. */
    ofptr = proc_getoftptr(fd);
    if (ofptr < 0) {
        return EBADF;
    }

    /* retrieve vnode pointer and file pointer from global open file table. */
    v_ptr = global_oft->open_files[ofptr]->v_ptr;

    seekable = VOP_ISSEEKABLE(v_ptr);
    if (!seekable) {
        return ESPIPE;
    }

    /* update the file pointer in global oft and save the new return file pointer in new_fp */
    err = upd_global_oft(ofptr, offset, whence, new_fp);
    if (err) {
        return err;
    }

    return 0;
}

/*
 * clones the file handle oldfd onto the file handle newfd. 
 */
int sys_dup2(int old_fd, int new_fd, int *retval) {
    struct vnode *v_ptr = NULL;
    off_t fp = -1;
    int ofptr, dup_ofptr, err;

    /* initialise the return value to an invalid value. */
    *retval = -1;

    /* if old_fd is the same as new_fd the function has no effect. */
    if (old_fd == new_fd) {
        *retval = new_fd;
        return 0;
    }

    /* retrieve old_fd open file ptr from process open file table. */
    ofptr = proc_getoftptr(old_fd);
    if (ofptr < 0 || new_fd < 0 || new_fd >= OPEN_MAX) {
        return EBADF;
    }

    /* check if the new_fd holds an open file, close if it does. */
    dup_ofptr = proc_getoftptr(new_fd);
    if (dup_ofptr != FREE_SLOT) {
        err = sys_close(new_fd, retval);
        if (err) {
            return err;
        }

    }

    err = add_global_oft(fp, v_ptr, &ofptr);
    if (err) {
        return err;
    }

    /* assign the ofptr of the old fd to the new fd. */
    err = proc_addfd(ofptr, &new_fd);
    if (err) {
        *retval = -1;
        return err;
    }

    *retval = new_fd;
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
    ofptr = -1;
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
    ofptr = -1;
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

    /* case when dup2 is simply incrementing the reference count */
    if (*ofptr >= 0) {
        lock_acquire(global_oft->oft_lock);
        KASSERT(global_oft->open_files[*ofptr] != NULL);
        global_oft->open_files[*ofptr]->ref_count++;
        lock_release(global_oft->oft_lock);
        return 0;
    }

    KASSERT(fp >= 0);

    /* create new file entry and save fp, v_ptr and initiate ref_count to 1 */
    new_file = (struct of_entry *) kmalloc(sizeof(struct of_entry));
    if (new_file == NULL) {
        return ENOMEM;
    }
    new_file->fp = fp;
    new_file->v_ptr = v_ptr;
    new_file->ref_count = 1;

    /* place the new file at the first available free slot */
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
 * decrement ref_count if open file is accessed by other file descriptors, remove otherwise.
 */
int rem_global_oft(int ofptr) {
    struct vnode *v_ptr = NULL;

    KASSERT(ofptr >= 0 && ofptr < OPEN_MAX);

    lock_acquire(global_oft->oft_lock);
    if (global_oft->open_files[ofptr]->ref_count > 1) {
        /* open file is still being used */
        global_oft->open_files[ofptr]->ref_count--;
    } else {
        /* open file entry can be removed */
        v_ptr = global_oft->open_files[ofptr]->v_ptr;
        /* use virtual file system call to close vnode */
        vfs_close(v_ptr);
        /* free entry in global open file table */
        kfree(global_oft->open_files[ofptr]);
        global_oft->open_files[ofptr] = NULL;
    }
    lock_release(global_oft->oft_lock);

    return 0;
}

/*
 * update file pointer at entry ofptr.
 */
int upd_global_oft(int ofptr, off_t offset, int whence, off_t *new_fp) {
    struct vnode *v_ptr = NULL;
    struct stat file_stat;
    int err = 0;

    switch (whence) {
        /* new file pointer is 0 (start of file) + offset */
        case SEEK_SET:
        *new_fp = offset;
        if (*new_fp < 0) {
            err = EINVAL;
            break;
        }
        lock_acquire(global_oft->oft_lock);
        global_oft->open_files[ofptr]->fp = *new_fp;
        lock_release(global_oft->oft_lock);
        break;

        /* new file pointer is current file pointer + offset (used for read/write updates)*/
        case SEEK_CUR:
        lock_acquire(global_oft->oft_lock);
        *new_fp = global_oft->open_files[ofptr]->fp + offset;
        if (*new_fp < 0) {
            lock_release(global_oft->oft_lock);
            err = EINVAL;
            break;
        }
        global_oft->open_files[ofptr]->fp = *new_fp;
        lock_release(global_oft->oft_lock);
        break;

        /* new file pointer is file size (end of file location) + offset */
        case SEEK_END:
        lock_acquire(global_oft->oft_lock);
        v_ptr = global_oft->open_files[ofptr]->v_ptr;
        /* retrieve file size into location file_stat->st_size */
        VOP_STAT(v_ptr, &file_stat);
        *new_fp = file_stat.st_size + offset;
        if (*new_fp < 0) {
            lock_release(global_oft->oft_lock);
            err = EINVAL;
            break;
        }
        global_oft->open_files[ofptr]->fp = *new_fp;
        lock_release(global_oft->oft_lock);
        break;

        /* whence is invalid */ 
        default:
        err = EINVAL;
        break;
    }
    
    if (err) {
        /* set new file pointer back to invalid value if an error occurred */
        *new_fp = -1;
        return err;
    }

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
