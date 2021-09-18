#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

#define BUG_ON(cond) if (cond) exit(2)

static inline void xdie(const char* fn_name)
{
	die("Cannot become a daemon [%s()]", fn_name);
}

static void close_fds() 
{
	const int posix_open_max = 20;
	int maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd == -1) {
		if (errno > 0)
			xdie("sysconf");
		maxfd = posix_open_max; 
	}
	for ( ; maxfd > 2; maxfd--)
		close(maxfd);
	errno = 0;
}

static void wait_child_notif_and_exit(int pfds[2])
{
	char notif;
	if (read(pfds[0], &notif, 1) == -1)
		xdie("read");
	_exit(0);
}

static void write_pid() 
{
	FILE* pidf = fopen("/run/vssd.pid", "w");
	if (pidf == NULL)
		xdie("fopen");
	fprintf(pidf, "%i", getpid());
	fclose(pidf);
}

static void notify_parent(int pfds[2])
{
	if (write(pfds[1], "n", 1) == -1)
		xdie("write");
	close(pfds[0]);
	close(pfds[1]);
}

static inline int connect_stdioe_to_null()
{
	close(0);
	close(1);
	close(2);
	int nullfd = open("/dev/null", O_RDWR);
	BUG_ON(nullfd != STDIN_FILENO);
	BUG_ON(dup2(nullfd, STDOUT_FILENO) != STDOUT_FILENO);
	BUG_ON(dup2(nullfd, STDERR_FILENO) != STDERR_FILENO);
}

void become_daemon()
{
	close_fds();
	int pfds[2];
	if (pipe(pfds) == -1)
		xdie("pipe");
	switch (fork()) {
		case -1: xdie("fork");
		case  0: break;
		default: wait_child_notif_and_exit(pfds);
	}
	if (setsid() == -1)
		xdie("setsid");
	switch (fork()) {
		case -1: xdie("fork");
		case  0: break;
		default: _exit(0);
	}
	write_pid();
	notify_parent(pfds);
	umask(0);
	if (chdir("/") == -1)
		xdie("chdir");
	connect_stdioe_to_null();
}

