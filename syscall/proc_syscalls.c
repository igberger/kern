#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include "opt-A2.h"

#include <vfs.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */


#if OPT_A2
/*THIS CODE DOES NOT COMPILE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void entrypoint(void *arg1, unsigned long arg2) {
  struct trapframe *tf, *newtf;
  struct addrspace addr;

  tf = (struct *trapframe) arg1;
  addr = (struct addrspace) arg2;

  curthread->t_addrspace = addr;
  as_activate(addr);

  newtf = tf;
  mips_usermode(newtf);
}


int
sys_fork(struct trapframe *tf, pid_t *retval){
  struct thread *child_thread;
  int result = 0;

  //copy parent's address space
  struct *child_addrspace;
  result = as_copy(curthread->addrspace, &child_addrspace);
  if(result) {
    return ENOMEM;
  }

  //copy parent's trapframe
  result = struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
  if(result) {
    return ENOMEM;
  }
  *child_tf = *tf;

  //create child thread
  result = thread_fork("Child Thread", entrypoint, (struct trapframe *) child_tf, 
    (unsigned long) child_addrspace, &child_thread);
  if(result) {
    return ENOMEM;
  }
  
}*/

/*
int
sys_execv(userptr_t prog, userptr_t args){
  return 0;
}
*/
#endif


void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  #if OPT_A2
  p->exitcode = _MKWAIT_EXIT(exitcode);
  p->is_exit = true;

  lock_acquire(p->p_waitpid);
  cv_broadcast(p->p_waitpid_cv, p->p_waitpid);
  lock_release(p->p_waitpid);
  #endif

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
  *retval = curproc->pid;
  
  #else
  *retval = 1;
  #endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

  #if OPT_A2
  struct proc *thisproc = curproc;
  bool mychild = is_proc_child(thisproc, pid);
  if(!mychild){
    kprintf("Error not child of current process");
    return 0;
  }

  struct proc *waitingproc = proc_pid_get(pid);
  lock_acquire(waitingproc->p_waitpid);
  while(waitingproc->is_exit == false){
    cv_wait(waitingproc->p_waitpid_cv, waitingproc->p_waitpid);
  }
  lock_release(waitingproc->p_waitpid);
  
  exitstatus = waitingproc->exitcode;

  #else
  exitstatus = 0;
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

