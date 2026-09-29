#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
int main(void) {
	printf("Padre: PID %d\n", getpid());
	for (int i = 0; i < 3; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			printf("Hijo %d: PID %d, PPID %d\n", i, getpid(), getppid());
			sleep(20);
			exit(0);
		}
	}
	for (int i = 0; i < 3; i++) wait(NULL);
	printf("Padre: los 3 hijos terminaron\n");
	return 0;
}
