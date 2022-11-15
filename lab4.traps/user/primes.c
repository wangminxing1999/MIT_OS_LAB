#include<user/user.h>
#include "kernel/types.h"
#include "kernel/stat.h"
#include<sys/types.h>

void loopFork(int*input, int ready)
{
    pid_t pid;
    int fd[2];

    close(input[1]);

    pipe(fd);

    if((pid = fork())>0) {
        int buf[1];
        int prime = 0;
        if(ready == 0) {
            close(fd[0]);

            for(int i=2; i<=35; i++) {
                buf[0] = i;
                write(fd[1],buf,1);
            }

            close(fd[1]);
        }
        else {
            if(read(input[0], buf, 1)>0) {
                printf("prime %d\n",buf[0]);
                prime = buf[0];
            }
            else {
                exit(0);
            }
            close(fd[0]);
            while(read(input[0], buf, 1)>0) {
                if(buf[0]%prime != 0) {
                    write(fd[1], buf, 1);
                }
            }
            close(input[0]);
            close(fd[1]);
        }
        wait(0);
        exit(0);
    }

    else if(pid == 0) {
        loopFork(fd,1);
        exit(0);
    }
}

int main()
{
    int temp[1];
    loopFork(temp,0);
    wait(0);
    exit(0);

}