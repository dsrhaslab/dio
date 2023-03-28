#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

int save_pid() {
	int pid = getpid();

	printf("app | Pid: %d\n", pid);
	FILE *pid_fd = fopen( "client-app.pid", "wx" );
	if (!pid_fd) {
		fprintf(stderr, "app | Failed to open file client-app.pid: %s\n", strerror(errno));
		return -1;
	}

	fprintf(pid_fd, "%d", pid);

	if (fclose(pid_fd) != 0) {
		fprintf(stderr, "app | Failed to close file client-app.pid: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int log_first_file(char *filename) {
	int fd1 = open(filename, O_CREAT|O_WRONLY|O_APPEND, 0666);
	if (fd1 < 0) {
		fprintf(stderr, "app | Failed to open file %s: %s\n", filename, strerror(errno));
		return -1;
	} else {
		printf("app | Created file %s\n", filename);
	}

	int ret = write(fd1, "This is the fist log line\n", 26);
	if (ret < 0) {
		fprintf(stderr, "app | Failed to write to file %s: %s\n", filename, strerror(errno));
		return -1;
	} else {
		printf("app | Wrote %d bytes to file %s\n", ret, filename);
	}

	if (close(fd1) != 0) {
		fprintf(stderr, "app | Failed to close file %s: %s\n", filename, strerror(errno));
		return -1;
	}

	printf("app | Sleeping for 10 seconds...\n");
	sleep(10);

	if (remove("/fluent-bit/tests/mnt/app.log") == 0)
		printf("app | Removed file %s\n", filename);
   	else {
     	fprintf(stderr, "Unable to remove file %s: %s\n", filename, strerror(errno));
     	return -1;
   	}

  	return 0;
}

int log_second_file(char *filename) {
	int fd2 = open(filename, O_CREAT|O_WRONLY|O_APPEND, 0666);
	if (fd2 < 0) {
		fprintf(stderr, "app | Failed to open file %s: %s\n", filename, strerror(errno));
		return -1;
	} else {
		printf("app | Created file %s\n", filename);
	}

	int ret = write(fd2, "Some new content\n", 16);
	if (ret < 0) {
		fprintf(stderr, "app | Failed to write to file %s: %s\n", filename, strerror(errno));
		return -1;
	} else {
		printf("app | Wrote %d bytes to file %s\n", ret, filename);
	}

	if (close(fd2) != 0) {
		fprintf(stderr, "app | Failed to close file %s: %s\n", filename, strerror(errno));
		return -1;
	}

	return 0;
}

int run_test() {
	if (log_first_file("/fluent-bit/tests/mnt/app.log") < 0) {
		perror("app | Failed to write to first log file");
		exit(1);
	}

	printf("app | Sleeping for 10 seconds...\n");
	sleep(10);
	if (log_second_file("/fluent-bit/tests/mnt/app.log") < 0) {
		perror("app | Failed to write to first log file");
		exit(1);
	}
}

void signalHandler(int signalNum) {
	printf("app | Caught signal %d. Starting...\n", signalNum);
}

int main() {
	signal(SIGUSR1, signalHandler);

	if (save_pid() < 0) {
		perror("app | Failed to write pid to file");
		exit(1);
	}

	printf("app | Waiting signal SIGUSR1 to start...\n");
	pause();

	struct timeval  tv1, tv2;
	gettimeofday(&tv1, NULL);
	run_test();
	gettimeofday(&tv2, NULL);
	printf ("app | Total time = %f seconds\n",
         (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
         (double) (tv2.tv_sec - tv1.tv_sec));

	return 0;
}