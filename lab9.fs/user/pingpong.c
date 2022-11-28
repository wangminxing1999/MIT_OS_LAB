#include<user/user.h>
#include "kernel/types.h"
#include "kernel/stat.h"
#include<sys/types.h>
int main()
{
    int fd1[2], fd2[2];
    char buf1[1],buf2[1];
    char send = 'a';

    if(pipe(fd1) < 0 || pipe(fd2) < 0) {
        printf("create the pipe failed!\n");
        exit(1);
    }

    pid_t pid = fork();

    if(pid < 0) {
        printf("fork failed\n");
        exit(1);
    }

    else if(pid == 0) {
        close(fd1[1]);
        read(fd1[0],buf1,1);
        close(fd1[0]);
        printf("%d: received ping\n",getpid());
        close(fd2[0]);
        write(fd2[1],buf1,1);
        close(fd2[1]);
        exit(0);
    }

    else if(pid > 0) {
        close(fd1[0]);
        write(fd1[1],&send,1);
        close(fd1[1]);
        close(fd2[1]);
        read(fd2[0],buf2,1);
        close(fd2[0]);
        printf("%d: received pong\n",getpid());
    }

    exit(0);

    
}
