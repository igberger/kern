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
runprogram(char *progname, int argc, char **argv)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

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


	vaddr_t userStackPtr = stackptr;//working address of stack
	//kprintf("stackptr: %x\tuserStackPtr: %x\n", stackptr, userStackPtr);

	//Count bytes in argv array
	int *argLengths = (int*)kmalloc(argc*sizeof(int));
	int argBytes = 0;
	for(int i=0; i<argc; i++){
		argLengths[i] = 0;
		argBytes += 4;//include 4 bytes for pointer at argv[i]
		for(int j=0; argv[i][j]!='\0'; j++){
			argLengths[i]++;
			argBytes++;//byte for each char
		}
		argLengths[i]++;
		argBytes++;//include byte for null char
	}
	argBytes +=4;//include 4 bytes for null pointer at the end of argv

	userStackPtr = userStackPtr - argBytes;//'Allocate' user stack space for buffer
	//kprintf("argBytes: %d\tuserStackPtr: %x\n", argBytes, userStackPtr);
	char *argBuff = kmalloc(argBytes);
	//Copy argv into buff
	int buffIndex = 4*(argc+1);//start index after argv pointer array
	int stringAdrOffset = 0;
	for(int i=0; i<argc; i++){
		unsigned int argvPtrValue = userStackPtr+4*(argc+1) + stringAdrOffset;//Starting address of string at argv[i]
		memcpy(argBuff+i*4, &argvPtrValue, 4);//Copy argv[i] pointer into buff
		//copy strings into buff
		for(int j=0; j<argLengths[i]; j++){
			argBuff[buffIndex] = argv[i][j];
			buffIndex++;
		}
		stringAdrOffset += argLengths[i];//Increase offset by length of string at argv[i]
	}
	int nullPtrValue = (int)NULL;
	memcpy(argBuff+argc*4, &nullPtrValue, 4);//Copy null pointer at end of argv pointer array

	result = copyout(argBuff, (userptr_t)userStackPtr, argBytes);//Copy argBuff into user stack
	if(result) {
		return result;
	}
	/*for(int i=0; i<argBytes; i++){
		if(i<4*(argc+1)){
			kprintf("argBuff[%d-%d]:%x %x %x %x\n", i, i+3, argBuff[i], argBuff[i+1], argBuff[i+2], argBuff[i+3]);
			i+=3;
		}else{
			kprintf("argBuff[%d]:%c\n", i, argBuff[i]);
		}
	}*/
	kfree(argLengths);
	kfree(argBuff);


	/*
	userptr_t userStackPtr = (userptr_t)stackptr;//Starting address of stack
	userptr_t currentStackPtr = userStackPtr-4;//Working address of stack

	copyout(&currentStackPtr, userStackPtr, 4);//Put argv pointer to argv array at start of stack
	for(int i=0; i<argc; i++){
		copyout(&userStackPtr, userStackPtr+(i+1)*4, 4);
		int argBytes=0;
		for(int j=0; argv[i][j]!='\0'; j++){
			argBytes++;
		}
		argBytes++;
		//copyout(argv[i], stackptr, )
	}
	*/

	/* Warp to user mode. */
	enter_new_process(argc/*argc*/, (userptr_t)userStackPtr/*userspace addr of argv*/, userStackPtr, entrypoint);//userStackPtr currently is argv pointer
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
#else
int
runprogram(char *progname)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

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

	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
#endif
