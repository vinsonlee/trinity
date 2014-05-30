#pragma once

#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>

#include "epoll.h"
#include "eventfd.h"
#include "exit.h"
#include "files.h"
#include "locks.h"
#include "net.h"
#include "pipes.h"
#include "perf.h"
#include "syscall.h"
#include "types.h"

void create_shm(void);
void create_shm_arrays(void);
void init_shm(void);

struct shm_s {
	/* Various statistics. TODO: Move to separate struct */
	unsigned long total_syscalls_done;
	unsigned long successes;
	unsigned long failures;
	unsigned int running_childs;

	/* Counts to tell if we're making progress or not. */
	unsigned long previous_op_count;	/* combined total of all children */
	unsigned long *child_op_count;		/* these are per-child so we can see if
						   a child is making progress or not. */

	/* rng related state */
	unsigned int seed;
	unsigned int *seeds;

	/* Indices of syscall in syscall table that are active.
	 * All indices shifted by +1. Empty index equals to 0.
	 *
	 * 'active_syscalls' is only used on uniarch. The other two
	 * are only used on biarch. */
	int active_syscalls32[MAX_NR_SYSCALL];
	int active_syscalls64[MAX_NR_SYSCALL];
	int active_syscalls[MAX_NR_SYSCALL];
	unsigned int nr_active_syscalls;
	unsigned int nr_active_32bit_syscalls;
	unsigned int nr_active_64bit_syscalls;

	/* pids */
	pid_t mainpid;
	pid_t *pids;
	pid_t last_reaped;

	/* various per-child data */
	unsigned char *kill_count;
	struct timeval taint_tv;

	/* log file related stuff */
	FILE **logfiles;
	bool *logdirty;

	/* file descriptors, created in main, inherited in children */
	int pipe_fds[MAX_PIPE_FDS*2];
	int file_fds[NR_FILE_FDS];
	int perf_fds[MAX_PERF_FDS];
	int epoll_fds[MAX_EPOLL_FDS];
	int eventfd_fds[MAX_EPOLL_FDS];
	struct socketinfo sockets[NR_SOCKET_FDS];
	int current_fd;
	unsigned int fd_lifetime;

	/* The actual syscall records each child uses. */
	struct syscallrecord *syscall;
	struct syscallrecord *previous;

	/* used by sanitize routines as tmp storage. */
	unsigned long *scratch;

	/* per-child mmaps */
	struct map **mappings;
	unsigned int *num_mappings;

	/* locks */
	lock_t reaper_lock;
	lock_t syscalltable_lock;

	/* various flags. */
	enum exit_reasons exit_reason;
	bool dont_make_it_fail;
	bool spawn_no_more;
	bool ready;
};
extern struct shm_s *shm;

#define SHM_OK 0
#define SHM_CORRUPT 1
