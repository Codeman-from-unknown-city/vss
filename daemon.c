#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>

#define KILL_PARENT		\
switch (fork()) {		\
	case -1: return -1;	\
	case 0:  break; 	\
	default: _exit(0);	\
}				\

#define RET_ERR_IF(cond)	\
if (cond)			\
	return -1		\

#define POSIX_OPEN_MAX 20

#define PROGRAM_NAME "vssd" // video streaming server daemon

int become_daemon()
{
	KILL_PARENT // turning into a background process
	RET_ERR_IF(setsid() == -1); // the process becomes the leader of the new session
	KILL_PARENT // we make sure that the process does not become the leader of the session
	umask(0);
	RET_ERR_IF(chdir("/") == -1);
	int maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd == -1) {
		RET_ERR_IF(errno > 0);
		maxfd = POSIX_OPEN_MAX; 
	}
	for (int fd = 0; fd < maxfd; ++fd)
		close(fd);
	errno = 0; // close() may change errno
	int nullfd = open("/dev/null", O_RDWR);
	RET_ERR_IF(nullfd != STDIN_FILENO);
	RET_ERR_IF(dup2(nullfd, STDOUT_FILENO) != STDOUT_FILENO);
	RET_ERR_IF(dup2(nullfd, STDERR_FILENO) != STDERR_FILENO);
	openlog(PROGRAM_NAME, 0, LOG_LOCAL0);
	return 0;
}

