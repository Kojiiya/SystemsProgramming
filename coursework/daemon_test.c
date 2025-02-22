#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <syslog.h>

int main(void) {
    pid_t pid, sid;
    time_t timebuf;

    //fork child; will be actual daemon
    pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        //in parent, let's bail
        printf("Finished\n");
        exit(EXIT_SUCCESS);
    }

    //child:
    //set new session
    if ((sid = setsid()) < 0) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }

    //reset file mode
    umask(0);

    //close stdin, stdout, stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    //set log mask to log all msgs
    setlogmask(LOG_UPTO(LOG_INFO));

    //open syslog with custom identifier/options
    openlog("CW2_DAEMON1", LOG_PID, LOG_DAEMON);

    //log msg to syslog
    syslog(LOG_INFO, "Daemon started");
    syslog(LOG_INFO, "PID: %d", (int)pid);

    closelog();

    printf("finished");
    return 0;
}
