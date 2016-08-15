/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <limits.h>
#include <copyinout.h>
#include "opt-A2.h"

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */


#if OPT_A2
int
runprogram(char *progname, char** args, int nargs)
#else
int
runprogram(char *progname)
#endif
{

#if OPT_A2
	if(nargs>64){
		return E2BIG;
	}
#endif

	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

#if OPT_A2
	int num_arg = nargs;
	char* arg_string = kmalloc(ARG_MAX);
	size_t *arr_offset = kmalloc(num_arg * sizeof(size_t));

        size_t temp_offset = 0;
	for(int i=0; i<num_arg; i++){
		strcpy(arg_string + temp_offset, args[i]);
		arr_offset[i] = temp_offset;
		temp_offset = temp_offset + ROUNDUP(strlen(args[i]) +1, 8);
	}
#endif

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

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

#if OPT_A2
	vaddr_t string_top = USERSTACK - temp_offset;
	//vaddr_t string_top = stackptr - temp_offset;
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

	kfree(arg_string);
	kfree(arr_offset);
	kfree(arr_offset_top);

        /* Warp to user mode. */
        enter_new_process(num_arg /*argc*/, (userptr_t)arr_top_addr /*userspace addr of argv*/,
                          arr_top_addr, entrypoint);
#else
	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
#endif	


	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

