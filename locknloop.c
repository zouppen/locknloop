/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2023 Joel Lehtonen
 *
 * NFSv4 compatible file locker which passes the lock to the kernel
 * loop device. In case of locking failure, error is reported.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <err.h>
#include <getopt.h>

#ifndef MSG_TIMEOUT
#define MSG_TIMEOUT "File is still locked!"
#endif
#ifndef MSG_LOCKED_NB
#define MSG_LOCKED_NB "The file is locked by another process!"
#endif
#ifndef MSG_WAIT
#define MSG_WAIT "File is currently locked by another process. Waiting for %ld seconds..."
#endif

void alarm_handler(int signum);
long parse_timeout(char const *s);
void help(int exitcode);

char const* bin_name;

void alarm_handler(int signum)
{
	errx(1, MSG_TIMEOUT);
}

long parse_timeout(char const *s) {
	char *end;
	long val = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || val < 0) {
		errx(3, "Invalid timeout value");
	}
	return val;
}

void help(int exitcode) {
	fprintf(stderr, "Usage: %s [-n|--no-lock] [-t|--timeout TIMEOUT] [-h|--help] LOCKFILE ]\n", bin_name);
	exit(exitcode);
}

int main(int argc, char **argv)
{
	bin_name = argv[0];
	bool do_lock = true;
	long wait_sec = 0;
	
	while (true) {
		int c;
		int option_index = 0;
		static struct option long_options[] = {
			{"help",    no_argument,       0, 'h'},
			{"no-lock", no_argument,       0, 'n'},
			{"timeout", required_argument, 0, 't'},
			{0,         0,                 0,  0 }
		};
		
		c = getopt_long(argc, argv, "hnt:",
				long_options, &option_index);
		if (c == -1) {
			// All processed
			break;
		}

		switch (c) {
		case 'h':
			help(0);
			break;
		case 'n':
			do_lock = false;
			break;
		case 't':
			wait_sec = parse_timeout(optarg);
			break;
		default:
			exit(2);
			break;
		}
	}

	if (argc != optind + 1) {
		help(2);
	}

	int fd = open(argv[optind], O_WRONLY);
	if (fd == -1) {
		err(3, "Unable to open %s", argv[optind]);
	}

	if (do_lock) {
		// Lock the whole file
		struct flock lock = {
			.l_type = F_WRLCK,
			.l_whence = SEEK_SET,
			.l_start = 0,
			.l_len = 0,
			.l_pid = 0,
		};

		// First, try locking without blocking to show if we are waiting
		bool got_lock = fcntl(fd, F_OFD_SETLK, &lock) != -1;
		if (!got_lock && errno != EWOULDBLOCK) {
			err(3, "Locking failure");
		}

		if (got_lock) {
			// We already got the lock
		} else if (wait_sec == 0) {
			// User requested non-blocking action, so quitting
			errx(1, MSG_LOCKED_NB);
		} else {
			// Wait for the lock with a timeout
			warnx(MSG_WAIT, wait_sec);
		
			if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
				err(3, "Unable to set signal handler");
			}
			alarm(wait_sec);
			int lock_state = fcntl(fd, F_OFD_SETLKW, &lock);
			alarm(0);

			if (lock_state == -1) {
				err(3, "Locking failure");
			}
		}
	}

	// Prepare the loop device
	int loopctlfd = open("/dev/loop-control", O_RDWR);
	if (loopctlfd == -1) {
		err(3, "Unable to open: /dev/loop-control");
	}

	long devnr = ioctl(loopctlfd, LOOP_CTL_GET_FREE);
	if (devnr == -1) {
		err(3,"ioctl-LOOP_CTL_GET_FREE");
	}

	char *loopname;
	if (asprintf(&loopname, "/dev/loop%ld", devnr) == -1) {
		err(3, "Memory allocation failed");
	}

	int loopfd = open(loopname, O_RDWR);
	if (loopfd == -1) {
		err(3, "Unable to open loop %s", loopname);
	}

	if (ioctl(loopfd, LOOP_SET_FD, fd) == -1) {
		err(3, "Unable to set up loop device");
	}

	if (ioctl(loopfd, LOOP_SET_DIRECT_IO, 1) == -1) {
		warnx("Opened the loop device without direct I/O");
	}

	printf("%s\n", loopname);

	exit(EXIT_SUCCESS);
}
