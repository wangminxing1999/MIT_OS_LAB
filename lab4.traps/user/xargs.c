#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
int main(int argc, char* argv[])
{
    char* store[MAXARG];
    char c = 0;
    int stat = 1;

    for(int i=0; i<argc; i++) {
        store[i] = argv[i+1];
    }

    while(stat)
    {
        char buf[40];
        int buf_end = 0;
        int count = argc - 1;

        while(1) {

            stat = read(0,&c,1);

            if(c == ' ' || c == '\n') {
                buf[buf_end] = 0;
                store[count] = buf;
                count ++;
                if(c == '\n') {
                    break;
                }
            }
            else {
                buf[buf_end]= c;
                buf_end++;
            }
        }

        if(fork() == 0) {
            exec(store[0],store);
        }
        else {
            wait(0);
        }
        
    }


    exit(0);
}