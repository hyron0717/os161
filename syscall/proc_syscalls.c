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
#include <mips/trapframe.h>
#include <spl.h>
#include <limits.h>
#include <vm.h>
#include <vfs.h>
#include <kern/fcntl.h>

#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

#if OPT_A2
  lock_acquire(proctable_lock);

  int index = get_node(curproc->pid);
  struct node* cur = array_get(proctable,index);

  if(cur->parent != 0){
    cur->status = 2;
    cur->exitcode = _MKWAIT_EXIT(exitcode);
    cv_broadcast(wait_cv, proctable_lock);
  }
  else{
    cur->status = 0;
    array_add(recycletable, &cur->pid, NULL);
  }

  for(unsigned int i=0; i< array_num(proctable); i++){
    struct node *temp = array_get(proctable,i);
    if((temp->parent == cur->pid) && (temp->status == 2)){
      temp->status = 0;
      temp->parent = 0;
      array_add(recycletable, &temp->pid, NULL);
    }
  }

  lock_release(proctable_lock);
#endif //OPT_A2


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
  KASSERT(curproc!=NULL);
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
/*
  int index = get_node(pid);
  struct node* cur = array_get(proctable,index);
  struct semaphore* sem = cur->nsem;
  if(sem == NULL){
    return ECHILD;
  }
  P(sem);
  exitstatus = cur->exitcode;
*/
  lock_acquire(proctable_lock);

  int index = get_node(pid);
  struct node* cur = array_get(proctable,index);

  struct proc* parent = curproc;

  if(cur==NULL){
    lock_release(proctable_lock);
    return ESRCH;
  }
  else if(parent->pid != cur->parent){
    lock_release(proctable_lock);
    return ECHILD;
  }
  
  while(cur->status == 1){
    cv_wait(wait_cv, proctable_lock);
  }

  exitstatus = cur->exitcode;
  lock_release(proctable_lock);
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif //OPT_A2

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}



#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval){
  KASSERT(curproc != NULL);

  int check;

  struct proc* curProc = curproc;
  struct proc* newp = proc_create_runprogram(curProc->p_name);

  if(newp == NULL){
    return ENOMEM;
  }

  int index = get_node(newp->pid);
  struct node* temp = array_get(proctable,index);
  temp->parent = curProc->pid;

  //copy address space
  struct addrspace *new_as;
  check = as_copy(curproc_getas(), &new_as);

  if(check!=0){
    proc_destroy(newp);
    return check;
  }

  newp->p_addrspace = new_as;

  //copy trapframe
  struct trapframe* new_tf = kmalloc(sizeof(struct trapframe));
  
  if(new_tf == NULL){
    proc_destroy(newp);
    return ENOMEM;
  }
  
  *new_tf = *tf;
 
  check = thread_fork(curthread->t_name, newp, enter_forked_process, new_tf, 0);

  if(check!=0){
    proc_destroy(newp);
    kfree(new_tf);
    return check;
  }
  
  *retval = newp->pid;
  return(0);
}



//

int sys_execv(userptr_t progname, userptr_t args){
  int result;
	
  if(progname==NULL || args ==NULL){
    return EFAULT;
  }

  int num_arg=0;
  int count=0;
  char * temp;

  //copy
  char ** arr_arg_ptr = kmalloc(ARG_MAX);
  while(true){
    result = copyin(args + count*sizeof(char*), &temp, sizeof(char *));

    if(result != 0){
      return result;
    }

    arr_arg_ptr[count] = temp;
    if(temp == NULL){
      break;
    }
    else{
      count = count + 1;
      num_arg = num_arg + 1;
    }
  }

  if(num_arg > 64){
    return E2BIG;
  }

  char* name = kmalloc(PATH_MAX);
  size_t actual_len;

  result = copyinstr(progname, name, PATH_MAX, &actual_len);
  if(result != 0){
    return result;
  }

  char* arg_string = kmalloc(ARG_MAX);
  size_t *arr_offset = kmalloc(num_arg * sizeof(size_t));

  size_t temp_offset = 0;
  for(int i=0; i<num_arg; i++){
    size_t arg_len;

    result = copyinstr((userptr_t) arr_arg_ptr[i], arg_string + temp_offset, ARG_MAX - temp_offset, &arg_len);
    if(result != 0){
      return result;
    }

    arr_offset[i] = temp_offset;
    temp_offset = temp_offset + ROUNDUP(arg_len + 1, 8);
  }

  // old address space
  struct addrspace *old_as = curproc_getas();

  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;

  /* Open the file. */
  result = vfs_open(name, O_RDONLY, 0, &v);
  if (result) {
    return result;
  }

  /* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    return ENOMEM;
  }

  /* Switch to it and activate it. */
  curproc_setas(as);
  as_activate();

  /* Load the executable. */
  result = load_elf(v, &entrypoint);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    vfs_close(v);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);

  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    return result;
  }

  vaddr_t string_top = USERSTACK - temp_offset;
//  vaddr_t string_top = stackptr - temp_offset;
  // copy argument string
  result = copyout(arg_string, (userptr_t) string_top, temp_offset);
  if(result != 0){ // copyout return 0 if success
    return result;
  }

  // argument array
  userptr_t * arr_offset_top = kmalloc(sizeof(userptr_t) * (num_arg+1));
  for(int i=0; i<num_arg; i++){
    userptr_t temp_userptr = (userptr_t) string_top + arr_offset[i];
    arr_offset_top[i] = temp_userptr;
  }
  arr_offset_top[num_arg] = NULL;


  vaddr_t arr_top_addr = string_top - (sizeof(userptr_t) * (num_arg + 1));

  result = copyout(arr_offset_top, (userptr_t) arr_top_addr, sizeof(userptr_t) * (num_arg + 1));
  if(result != 0){ // copyout return 0 if success
    return result;
  }


  as_destroy(old_as);
  kfree(arr_arg_ptr);
  kfree(name);
  kfree(arg_string);
  kfree(arr_offset);
  kfree(arr_offset_top);

  /* Warp to user mode. */
  enter_new_process(num_arg /*argc*/, (userptr_t)arr_top_addr /*userspace addr of argv*/,arr_top_addr, entrypoint);

  /* enter_new_process does not return. */
  panic("enter_new_process returned\n");
  return EINVAL;
}


#endif //OPT_A2

