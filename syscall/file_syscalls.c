#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>
#include <kern/iovec.h>
#include <lib.h>
#include <uio.h>
#include <syscall.h>
#include <vnode.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>
#include "opt-A2.h"
#include <copyinout.h>
#include <filetable.h>
#include <current.h>
#include <vm.h>
#include <limits.h>
#include <addrspace.h>
#include <synch.h>

/* handler for write() system call                  */
/*
 * n.b.
 * This implementation handles only writes to standard output 
 * and standard error, both of which go to the console.
 * Also, it does not provide any synchronization, so writes
 * are not atomic.
 *
 * You will need to improve this implementation
 */
 
#if OPT_A2
int
sys_open(userptr_t filenameptr, int flags, int mode, int *ret){
    int result;
    int err;
    char *filename = (char *)filenameptr;
    mode = mode + 1;
    
    if(filename == NULL || (unsigned int)filename == 0x40000000) {
        *ret = EFAULT;
        return -1;
    }  
    //check if the filename address is in kernel
    if((unsigned int)filename >= 0x80000000){
        if ((unsigned int)filename < 0xffffffff){
            if(strcmp(filename,"con:")){
                    *ret = EFAULT;
                    return -1;
            }
        }
    }
    //check if the filename is invalid pointer
  
    if((flags & O_RDWR) && vm_invalidaddress((vaddr_t)filename)) {
        *ret = EFAULT;
        return -1;
    }
    
    if (strlen(filename) == 0) {
        * ret = EINVAL;
        return -1;
    }

    /* check valid combinations of flags */
    if (flags & O_APPEND) {
        *ret = EFAULT;
        return -1;
    }
    if (flags & O_EXCL) {
        if (!flags & O_CREAT) {
            *ret = EFAULT;
            return -1;
        }
    }
    //reserve 0,1,2 for stdio
    if(!curproc->stdio_reserve){
        curproc->stdio_reserve = true;
        err = open("con:", O_RDONLY,ret);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,ret);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,ret);
        if(err){
            return -1;
        }
    }
    struct vnode *retvnode;  
    char* temp_filename;

    temp_filename = kstrdup(filename);
    result = vfs_open(temp_filename,flags,0,&retvnode);
    if (result) {
        *ret = result;
        return -1;
    }
    
    int fdesc;
    
    result = create_file_and_add_to_table(curproc->p_ft, retvnode, flags, &fdesc);
    if (result) {
        *ret = result;
        return -1;
    }

    *ret = fdesc;

    return 0;
}

int
sys_read(int fdesc, userptr_t ubuf, unsigned int nbytes, int *retval){
    off_t data; // data have read by kernel
    struct File *tempfile = NULL;
    struct iovec iov;
    struct uio u;
    int err;
    //check if buffer is invalid
    if(vm_invalidaddress((vaddr_t)ubuf)){
        *retval = EFAULT;
        return -1;
    }
    if(fdesc >= OPEN_MAX){
        *retval = EBADF;
        return -1;
    }
    if(!curproc->stdio_reserve){
        curproc->stdio_reserve = true;
        err = open("con:", O_RDONLY,retval);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,retval);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,retval);
        if(err){
            return -1;
        }
    }
    
    tempfile = curproc->p_ft->files[fdesc];
    if(tempfile == NULL){
        *retval = EBADF;
        return -1;
    }
    if(tempfile->flags == O_WRONLY){
        *retval = EBADF;
        return -1;
    }
    lock_acquire(tempfile->rw_lock);
    uio_kinit(&iov, &u, ubuf, nbytes, tempfile->offset, UIO_READ);
    VOP_READ(tempfile->vn,&u);
    data = nbytes - u.uio_resid;
    tempfile->offset = u.uio_offset;
    *retval = data;
    lock_release(tempfile->rw_lock);
    return 0;

}

int
sys_close(int fdesc, int32_t *retval){
  int result;
  struct FileTable *curFt = curproc->p_ft;

  if(fdesc < 3 || fdesc > 127) {
    *retval = EBADF;
    return -1;
  }

  result = close_file_and_remove_from_table(curFt, fdesc);
  if(result){
    *retval = EBADF;
    return -1;
  }
  return result;
}
#endif

int
sys_write(int fdesc,userptr_t ubuf,unsigned int nbytes,int *retval)
{
  #if OPT_A2

    struct File *tempfile = NULL;
    off_t data;
    struct iovec iov;
    struct uio u;
    int err;
  if(fdesc<=0 || fdesc >= OPEN_MAX){
        *retval = EBADF; 
        return -1;
    }
    
    if(vm_invalidaddress((vaddr_t)ubuf)) {
        *retval = EFAULT;
        return -1;
    }
    
    if(!curproc->stdio_reserve){
        curproc->stdio_reserve = true;
        err = open("con:", O_RDONLY,retval);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,retval);
        if(err){
            return -1;
        }
        err = open("con:", O_WRONLY,retval);
        if(err){
            return -1;
        }
    }
    
    
    
    tempfile = curthread->t_proc->p_ft->files[fdesc];
    if(tempfile == NULL){
        *retval = EBADF;
        return -1;
    }
    if(tempfile->flags == O_RDONLY){
        *retval = EBADF;
        return -1;
    }
    
    lock_acquire(tempfile->rw_lock);
    
    uio_kinit(&iov, &u, (void*)ubuf, nbytes, tempfile->offset, UIO_WRITE);

    if(fdesc > 0){
        err = VOP_WRITE(tempfile->vn,&u);
        if(err){
            *retval = err;
            return -1;
        }
    }
    data = nbytes - u.uio_resid;
    tempfile->offset = u.uio_offset;
    *retval = data;
    
    lock_release(tempfile->rw_lock);
  return 0;

  #else
  struct iovec iov;
  struct uio u;
  int res;

  DEBUG(DB_SYSCALL,"Syscall: write(%d,%x,%d)\n",fdesc,(unsigned int)ubuf,nbytes);
  
  /* only stdout and stderr writes are currently implemented */
  if (!((fdesc==STDOUT_FILENO)||(fdesc==STDERR_FILENO))) {
    return EUNIMP;
  }
  KASSERT(curproc != NULL);
  KASSERT(curproc->console != NULL);
  KASSERT(curproc->p_addrspace != NULL);

  /* set up a uio structure to refer to the user program's buffer (ubuf) */
  iov.iov_ubase = ubuf;
  iov.iov_len = nbytes;
  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_offset = 0;  /* not needed for the console */
  u.uio_resid = nbytes;
  u.uio_segflg = UIO_USERSPACE;
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace;

  res = VOP_WRITE(curproc->console,&u);
  if (res) {
    return res;
  }

  /* pass back the number of bytes actually written */
  *retval = nbytes - u.uio_resid;
  KASSERT(*retval >= 0);
  return 0;
  #endif
}
