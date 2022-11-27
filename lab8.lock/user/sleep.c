#include"user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"


int main(int argc, char *argv[])
{
    if(argc == 2) {
        int sleep_time = atoi(argv[1]);
        sleep(sleep_time);
        exit(0);
    }
    else {
        printf("not enough arguments!");
        exit(1);
    }
}
