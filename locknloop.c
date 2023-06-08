/**
 * NFSv4 compatible file locker. Forks process which holds the lock
 * while returning immediately. In case of locking failure, error is
 * reported.
 *
 * Compiling: klcc -Wall -s -static -o filelocker main-rangelock-klcc.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#ifndef MSG_TIMEOUT
#define MSG_TIMEOUT "File is still locked!"
#endif
#ifndef MSG_LOCKED_NB
#define MSG_LOCKED_NB "The file is locked by another process!"
#endif
#ifndef MSG_WAIT
#define MSG_WAIT "File is currently locked by another process. Waiting for %ld seconds..."
#endif

// Not included in klibc, so must include here
#define warnx(msg, ...) { fprintf(stderr, (msg "\n") __VA_OPT__(,) __VA_ARGS__); }
#define warn(msg, ...) { fprintf(stderr, (msg ": %s\n") __VA_OPT__(,) __VA_ARGS__, strerror(errno)); }
#define errx(n, ...) { warnx(__VA_ARGS__); exit((n)); }
#define err(n, ...) { warn(__VA_ARGS__); exit((n)); }

void alarm_handler(int signum);
long parse_int(char const *s);

void alarm_handler(int signum)
{
	errx(1, MSG_TIMEOUT);
}

long parse_int(char const *s) {
	char *end;
	long wait_sec = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || wait_sec < 0) {
		errx(3, "Invalid timeout value");
	}
	return wait_sec;
}

int main(int argc, char **argv)
{
	if (argc != 2 && argc != 3) {
		errx(2, "Usage: %s LOCKFILE [TIMEOUT]", argv[0]);
	}

	// Parse timeout
	long wait_sec = argc == 3 ? parse_int(argv[2]) : 0;
	
	int fd = open(argv[1], O_WRONLY);
	if (fd == -1) {
		err(3, "Unable to open %s", argv[1]);
	}

	// Lock the whole file
	struct flock lock = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
		.l_pid = 0,
	};

	// First, try locking without blocking to show if we are waiting
	bool ok = fcntl(fd, F_OFD_SETLK, &lock) != -1;
	if (!ok && errno != EWOULDBLOCK) {
		err(3, "Locking failure");
	}

	if (ok) {
		// We already got the lock
	} else if (wait_sec == 0) {
		// Waiting without timeout
		errx(1, MSG_LOCKED_NB);
	} else {
		// Wait with a timeout
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

	// Prepare the loop device
	int loopctlfd = open("/dev/loop-control", O_RDWR);
	if (loopctlfd == -1) {
		err(3, "Unable to open: /dev/loop-control");
	}

	// Due to klibc bug, third parameter is required here
	long devnr = ioctl(loopctlfd, LOOP_CTL_GET_FREE, 0);
	if (devnr == -1) {
		err(3,"ioctl-LOOP_CTL_GET_FREE");
	}

	char *loopname;
	asprintf(&loopname, "/dev/loop%ld", devnr);

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
