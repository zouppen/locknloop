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
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Not included in klibc, so must include here
#define errx(n, msg, ...) { fprintf(stderr, (msg "\n") __VA_OPT__(,) __VA_ARGS__); exit((n)); }
#define err(n, msg, ...) { fprintf(stderr, (msg ": %s\n") __VA_OPT__(,) __VA_ARGS__, strerror(errno)); exit((n)); }

int main(int argc, char **argv)
{
	if (argc < 2) {
		errx(2, "Usage: %s LOCKFILE [COMMANDS..]", argv[0]);
	}

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
	
	if (fcntl(fd, F_OFD_SETLK, &lock) == -1) {
		if (errno == EWOULDBLOCK) {
			errx(1, "%s is currently locked!", argv[1]);
		} else {
			err(3, "Locking failure");
		}
	}

	if (argc == 2) {
		// Holding the lock in a child process
		pid_t pid = fork();
		if (pid == -1) {
			err(3, "Unable to fork");
		} else if (pid == 0) {
			// I'm the child. We have no way to go back, so no
			// point in checking errors.

			// Closing file descriptors except the
			// lock. close_range should be used but we want to be
			// compatible with legacy linuxes, too.
			close(0);
			close(1);
			close(2);

			// Wait forever
			pause();
		} else {
			// Report the holding PID
			printf("%d\n", pid);
		}
	} else {
		// Exec mode (child holds the lock)
		execvp(argv[2], argv+2);
		err(4, "Unable to start process");
	}
}
