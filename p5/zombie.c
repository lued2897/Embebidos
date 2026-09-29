#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
 
int main(void) {
    pid_t pid = fork();
    if (pid == 0) exit(0);
    printf("Padre %d, hijo %d. Duermo 30 s sin llamar a wait()\n", getpid(), pid);
    sleep(30);
    wait(NULL);
    return 0;
}
