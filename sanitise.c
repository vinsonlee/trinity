#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "files.h"
#include "arch.h"
#include "scrashme.h"


static char * filebuffer = NULL;
static unsigned long filebuffersize = 0;

static unsigned long get_interesting_32bit_value()
{
	int i;
	i = rand() % 12;

	switch (i) {
	/* 32 bit */
	case 0:		return 0x00000001;
	case 1:		return 0x00008000;
	case 2:		return 0x0000ffff;
	case 3:		return 0x00010000;
	case 4:		return 0x7fffffff;
	case 5:		return 0x80000000;
	case 6:		return 0x80000001;
	case 7:		return 0x8fffffff;
	case 8:		return 0xf0000000;
	case 9:		return 0xff000000;
	case 10:	return 0xffffff00 | (rand() % 256);
	case 11:	return 0xffffffff;
	}
	/* Should never be reached. */
	return 0;
}

static unsigned long get_interesting_value()
{
	int i;
	unsigned long low;

#ifndef __64bit__
	return get_interesting_32bit_value();
#endif

	low = get_interesting_32bit_value();

	i = rand() % 13;

	switch (i) {
	/* 64 bit */
	case 0:	return 0;
	case 1:	return 0x0000000100000000;
	case 2:	return 0x7fffffff00000000;
	case 3:	return 0x8000000000000000;
	case 4:	return 0xffffffff00000000;
	case 5:	return low;
	case 6:	return low | 0x0000000100000000;
	case 7:	return low | 0x7fffffff00000000;
	case 8:	return low | 0x8000000000000000;
	case 9:	return low | 0xffffffff00000000;
	case 10: return (low & 0xffffff) | 0xffffffff81000000;	// x86-64 kernel text address
	case 11: return (low & 0x0fffff) | 0xffffffffff600000;	// x86-64 vdso
	case 12: return (low & 0xffffff) | 0xffffffffa0000000;	// x86-64 module space
	}
	/* Should never be reached. */
	return 0;
}


static unsigned long get_address()
{
	int i;

	i = rand() % 3;
	switch (i) {
	case 0:	return KERNEL_ADDR;
	case 1:	return (unsigned long) useraddr;
	case 2:	return get_interesting_value();
	}

	return 0;
}


static unsigned int get_pid()
{
	int i;
	i = rand() % 2;

	switch (i) {
	case 0:	return getpid();
	case 1:	return rand() & 32768;
	case 2: break;
	}
	return 0;
}


static unsigned long fill_arg(int call, int argnum)
{
	int fd;
	unsigned long i;
	unsigned long low=0, high=0;
	unsigned int num=0;
	unsigned int *values=NULL;
	unsigned int argtype=0;

	switch (argnum) {
	case 1:	argtype = syscalls[call].arg1type;
		break;
	case 2:	argtype = syscalls[call].arg2type;
		break;
	case 3:	argtype = syscalls[call].arg3type;
		break;
	case 4:	argtype = syscalls[call].arg4type;
		break;
	case 5:	argtype = syscalls[call].arg5type;
		break;
	case 6:	argtype = syscalls[call].arg6type;
		break;
	}

	switch (argtype) {
	case ARG_FD:
		fd = get_random_fd();
		//printf (YELLOW "DBG: %x" WHITE "\n", fd);
		return fd;
	case ARG_LEN:
		return get_interesting_value();
	case ARG_ADDRESS:
		return get_address();
	case ARG_PID:
		return get_pid();
	case ARG_RANGE:
		switch (argnum) {
		case 1:	low = syscalls[call].low1range;
			high = syscalls[call].hi1range;
			break;
		case 2:	low = syscalls[call].low2range;
			high = syscalls[call].hi2range;
			break;
		case 3:	low = syscalls[call].low3range;
			high = syscalls[call].hi3range;
			break;
		case 4:	low = syscalls[call].low4range;
			high = syscalls[call].hi4range;
			break;
		case 5:	low = syscalls[call].low5range;
			high = syscalls[call].hi5range;
			break;
		case 6:	low = syscalls[call].low6range;
			high = syscalls[call].hi6range;
			break;
		}
		i = rand64() % high;
		if (i < low) {
			i += low;
			i &= high;
		}
		return i;
	case ARG_LIST:
		switch (argnum) {
		case 1:	num = syscalls[call].arg1list.num;
			values = syscalls[call].arg1list.values;
			break;
		case 2:	num = syscalls[call].arg2list.num;
			values = syscalls[call].arg2list.values;
			break;
		case 3:	num = syscalls[call].arg3list.num;
			values = syscalls[call].arg3list.values;
			break;
		case 4:	num = syscalls[call].arg4list.num;
			values = syscalls[call].arg4list.values;
			break;
		case 5:	num = syscalls[call].arg5list.num;
			values = syscalls[call].arg5list.values;
			break;
		case 6:	num = syscalls[call].arg6list.num;
			values = syscalls[call].arg6list.values;
			break;
		}
		i = rand() % num;
		return values[i];
	}

	return 0x5a5a5a5a;	/* Should never happen */
}


void generic_sanitise(int call,
	unsigned long *a1,
	unsigned long *a2,
	unsigned long *a3,
	unsigned long *a4,
	unsigned long *a5,
	unsigned long *a6)
{
	if (syscalls[call].arg1type != 0)
		*a1 = fill_arg(call, 1);
	if (syscalls[call].arg2type != 0)
		*a2 = fill_arg(call, 2);
	if (syscalls[call].arg3type != 0)
		*a3 = fill_arg(call, 3);
	if (syscalls[call].arg4type != 0)
		*a4 = fill_arg(call, 4);
	if (syscalls[call].arg5type != 0)
		*a5 = fill_arg(call, 5);
	if (syscalls[call].arg6type != 0)
		*a6 = fill_arg(call, 6);
}



/*
 * asmlinkage ssize_t sys_read(unsigned int fd, char __user * buf, size_t count)
 */
void sanitise_read(
		__unused__ unsigned long *a1,
		unsigned long *a2,
		unsigned long *a3,
		__unused__ unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{
	unsigned long newsize = (unsigned int) *a3 >> 16;

	if (filebuffer != NULL) {
		if (filebuffersize < newsize) {
			free(filebuffer);
			filebuffersize = 0;
			filebuffer = NULL;
		}
	}

	if (filebuffer == NULL) {
retry:
		filebuffer = malloc(newsize);
		if (filebuffer == NULL) {
			newsize >>= 1;
			goto retry;
		}
		filebuffersize = newsize;
	}
	*a2 = (unsigned long) filebuffer;
	*a3 = newsize;
	memset(filebuffer, 0, newsize);
}

/*
 * asmlinkage ssize_t sys_write(unsigned int fd, char __user * buf, size_t count)
 */
void sanitise_write(
		__unused__ unsigned long *a1,
		unsigned long *a2,
		unsigned long *a3,
		__unused__ unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{
	unsigned long newsize = *a3 & 0xffff;
	void *newbuffer;

retry:
	newbuffer = malloc(newsize);
	if (newbuffer == NULL) {
		newsize >>= 1;
		goto retry;
	}

	free(filebuffer);
	filebuffer = newbuffer;
	filebuffersize = newsize;

	*a2 = (unsigned long) filebuffer;
	*a3 = newsize;
}


/*
 * sys_mprotect(unsigned long start, size_t len, unsigned long prot)
 */
#include <sys/mman.h>
#define PROT_SEM    0x8

void sanitise_mprotect(
		unsigned long *a1,
		unsigned long *a2,
		unsigned long *a3,
		__unused__ unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{
	unsigned long end;
	unsigned long mask = ~(page_size-1);
	int grows;

	*a1 &= mask;

retry_end:
	end = *a1 + *a2;
	if (*a2 == 0) {
		*a2 = rand64();
		goto retry_end;
	}

	/* End must be after start */
	if (end <= *a1) {
		*a2 = rand64();
		goto retry_end;
	}

retry_prot:
	*a3 &= ((PROT_GROWSDOWN|PROT_GROWSUP) | ~(PROT_READ | PROT_WRITE | PROT_EXEC | PROT_SEM));

	grows = *a3 & (PROT_GROWSDOWN|PROT_GROWSUP);

	if (grows == (PROT_GROWSDOWN|PROT_GROWSUP)) { /* can't be both */
		*a3 &= rand64();
		goto retry_prot;
	}
}


/*
 * asmlinkage long sys_rt_sigaction(int sig,
          const struct sigaction __user *act,
          struct sigaction __user *oact,
          size_t sigsetsize)
 */
#include <signal.h>

void sanitise_rt_sigaction(
		__unused__ unsigned long *a1,
		__unused__ unsigned long *a2,
		__unused__ unsigned long *a3,
		unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{
	*a4 = sizeof(sigset_t);
}

/*
 * asmlinkage long
 sys_rt_sigprocmask(int how, sigset_t __user *set, sigset_t __user *oset, size_t sigsetsize)
 */
void sanitise_rt_sigprocmask(
		__unused__ unsigned long *a1,
		__unused__ unsigned long *a2,
		__unused__ unsigned long *a3,
		unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{
	*a4 = sizeof(sigset_t);
}


/*
 * asmlinkage ssize_t sys_pread64(unsigned int fd, char __user *buf,
				                 size_t count, loff_t pos)
 */
void sanitise_pread64(
		__unused__ unsigned long *a1,
		__unused__ unsigned long *a2,
		__unused__ unsigned long *a3,
		unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{

retry_pos:
	if ((int)*a4 < 0) {
		*a4 = rand64();
		goto retry_pos;
	}
}

/*
 * asmlinkage ssize_t sys_pwrite64(unsigned int fd, char __user *buf,
				                 size_t count, loff_t pos)
 */
void sanitise_pwrite64(
		__unused__ unsigned long *a1,
		__unused__ unsigned long *a2,
		__unused__ unsigned long *a3,
		unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{

retry_pos:
	if ((int)*a4 < 0) {
		*a4 = rand64();
		goto retry_pos;
	}
}



/*
 * asmlinkage unsigned long sys_mremap(unsigned long addr,
 *   unsigned long old_len, unsigned long new_len,
 *   unsigned long flags, unsigned long new_addr)
 *
 * This syscall is a bit of a nightmare to fuzz as we -EINVAL all over the place.
 * It might be more useful once we start passing around valid maps instead of just
 * trying random addresses.
 */
#include <linux/mman.h>

void sanitise_mremap(
		unsigned long *addr,
		__unused__ unsigned long *old_len,
		unsigned long *new_len,
		unsigned long *flags,
		unsigned long *new_addr,
		__unused__ unsigned long *a6)
{
	unsigned long mask = ~(page_size-1);
	int i;

	*flags = rand64() & ~(MREMAP_FIXED | MREMAP_MAYMOVE);

	*addr &= mask;

	i=0;
	if (*flags & MREMAP_FIXED) {
		*flags &= ~MREMAP_MAYMOVE;
		*new_len &= TASK_SIZE-*new_len;
retry_addr:
		*new_addr &= mask;
		if ((*new_addr <= *addr) && (*new_addr+*new_len) > *addr) {
			*new_addr -= *addr - (rand() % 1000);
			goto retry_addr;
		}

		if ((*addr <= *new_addr) && (*addr+*old_len) > *new_addr) {
			*new_addr += *addr - (rand() % 1000);
			goto retry_addr;
		}

		/* new_addr > TASK_SIZE - new_len*/
retry_tasksize_end:
		if (*new_addr > TASK_SIZE - *new_len) {
			*new_addr >>= 1;
			i++;
			goto retry_tasksize_end;
		}
		printf("retried_tasksize_end: %d\n", i);
	}

	//TODO: Lots more checks here.
	// We already check for overlap in do_mremap()
}


/*
 * asmlinkage long sys_sync_file_range(int fd, loff_t offset, loff_t nbytes, unsigned int flags)
 * flags must be part of VALID_FLAGS (SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE| SYNC_FILE_RANGE_WAIT_AFTER)
 */

#define SYNC_FILE_RANGE_WAIT_BEFORE 1
#define SYNC_FILE_RANGE_WRITE       2
#define SYNC_FILE_RANGE_WAIT_AFTER  4

#define VALID_SFR_FLAGS (SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER)

void sanitise_sync_file_range(__unused__ unsigned long *a1, unsigned long *a2, unsigned long *a3, unsigned long *a4, __unused__ unsigned long *a5, __unused__ unsigned long *a6)
{

retry_flags:
	if (*a4 & ~VALID_SFR_FLAGS) {
		*a4 = rand64() & VALID_SFR_FLAGS;
		printf("retrying flags\n");
		goto retry_flags;
	}

retry_offset:
	if ((signed long)*a2 < 0) {
		*a2 = rand64();
		printf("retrying offset\n");
		goto retry_offset;
	}

	if ((signed long)*a2+(signed long)*a3 < 0)
		goto retry_offset;

	if (*a2+*a3 < *a2)
		goto retry_offset;
}

/*
 * asmlinkage long sys_set_robust_list(struct robust_list_head __user *head,
 *           size_t len)
*/
struct robust_list {
	struct robust_list *next;
};
struct robust_list_head {
	struct robust_list list;
	long futex_offset;
	struct robust_list *list_op_pending;
};

void sanitise_set_robust_list(
	__unused__ unsigned long *a1,
	unsigned long *len,
	__unused__ unsigned long *a3,
	__unused__ unsigned long *a4,
	__unused__ unsigned long *a5,
	__unused__ unsigned long *a6)
{
	*len = sizeof(struct robust_list_head);
}


/*
 * asmlinkage long sys_vmsplice(int fd, const struct iovec __user *iov,
 *                unsigned long nr_segs, unsigned int flags)
 */

void sanitise_vmsplice(
	__unused__ unsigned long *fd,
	__unused__ unsigned long *a2,
	unsigned long *a3,
	__unused__ unsigned long *a4,
	__unused__ unsigned long *a5,
	__unused__ unsigned long *a6)
{
	*a3 = rand() % 1024;	/* UIO_MAXIOV */
}

#include <sys/types.h>
#include <sys/socket.h>
void sanitise_sendto(__unused__ unsigned long *fd,
	__unused__ unsigned long *buff,
	__unused__ unsigned long *len,
	__unused__ unsigned long *flags,
	__unused__ unsigned long *addr,
	unsigned long *addr_len)
{
	*addr_len %= 128;	// MAX_SOCK_ADDR
}

void sanitise_fanotify_mark(
		__unused__ unsigned long *a1,
		__unused__ unsigned long *a2,
		unsigned long *a3,
		__unused__ unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{
	*a3 &= 0xffffffff;
}

void sanitise_remap_file_pages(
		unsigned long *start,
		unsigned long *size,
		__unused__ unsigned long *a3,
		__unused__ unsigned long *a4,
		unsigned long *pgoff,
		__unused__ unsigned long *a6)
{

	*start = *start & PAGE_MASK;
	*size = *size & PAGE_MASK;


retry_size:
	if (*start + *size <= *start) {
		*size = get_interesting_32bit_value() & PAGE_MASK;
		goto retry_size;
	}

retry_pgoff:
	if (*pgoff + (*size >> PAGE_SHIFT) < *pgoff) {
		*pgoff = get_interesting_value();
		goto retry_pgoff;
	}

retry_pgoff_bits:
	if (*pgoff + (*size >> PAGE_SHIFT) >= (1UL << PTE_FILE_MAX_BITS)) {
		*pgoff = (*pgoff >> 1);
		goto retry_pgoff_bits;
	}
}




void sanitise_ioctl(
		__unused__ unsigned long *a1,
		__unused__ unsigned long *a2,
		unsigned long *a3,
		__unused__ unsigned long *a4,
		__unused__ unsigned long *a5,
		__unused__ unsigned long *a6)
{
	int i;

	*a2 = rand() % 0xffff;
	i = rand() % 3;
	if (i == 1)
		*a2 |= 0x80044000;
	if (i == 2)
		*a2 |= 0xc0044000;

	*a3 = (rand() & 0xffffffff);
	i = rand() % 4;
	if (i == 1)
		*a3 &= 0xffff;
	if (i == 2)
		*a3 &= 0xffffff;
	if (i == 3)
		*a3 = get_interesting_32bit_value();

}
