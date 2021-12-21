/** Compilation: gcc -o ipc main.c producer.c consumer.c -lpthread -lrt **/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "shmem.h"

void report_and_exit(const char *msg)
{
	perror(msg);
	exit(-1);
}

void send_list_of_files(char *files[], int num_files)
{
	char *mysocketpath = IPC_SOCKET_NAME;
	int i;
	int first = 1;
	char comma = ';';
	struct sockaddr_un namesock;
	int socket2py;
	namesock.sun_family = AF_UNIX;

	/* Note that this is a named socket. We used it, since its hard to create unnamed socket and
	 * pass the other end to the Python process
	 */
	strncpy(namesock.sun_path, (char *)mysocketpath, sizeof(namesock.sun_path));

	/* Note that both socket ends, must use the same socket mode, either SOCK_STREAM, or SOCK_DGRAM.
	 * In this case, we select SOCK_STREAM
	 */
	socket2py = socket(AF_UNIX, SOCK_STREAM, 0);
	bind(socket2py, (struct sockaddr *)&namesock, sizeof(struct sockaddr_un));
	printf("Sending list of files, comma seperated\n");

	/* Before executing the connect, the Python on the other end, must already be
	 * ready with accept, hence we sleep 1 second before
	 */
	sleep(1);
	if (connect(socket2py, (struct sockaddr *)&namesock, sizeof(namesock)) == -1) {
		report_and_exit("connect");
	}

	for (i = 0; i < num_files; i++) {
		if (first) {
			first = 0;
		} else {
			send(socket2py, &comma, 1, 0);
		}
		send(socket2py, files[i], strlen(files[i]), 0);
	}
}
void main(int argc, char **argv)
{
	int fd;
	int s, i, pid, py_pid;
	pthread_t solver_thread_id;
	struct Question qbuf;
	int *p, *c;
	char *files[100];
	int num_files = 0;
	int num_child_processes = 0;
	struct solverShmem *solver_shmem_ptr;

	/* Get program arguments */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'f':
				files[num_files] = argv[++i];
				num_files++;
				break;
			default:
				report_and_exit("Unknown option provided.");
				break;
			}
		} else {
			char buf[256];
			sprintf(buf, "Unknown arg provided %s. Expected -f <file>\n", argv[i]);
			report_and_exit(buf);
		}
	}

	if (num_files == 0) {
		report_and_exit("No files were given");
	}

	/* Destory & Create full semaphore */
	sem_unlink(IPC_SEM_FULL_NAME);
	sem_open(IPC_SEM_FULL_NAME, /* name */
		 O_CREAT,       /* create the semaphore */
		 ACCESS_PERMS,   /* protection perms */
		 0); /* No cell is populated */

	/* Destory Create empty semaphore  */
	sem_unlink(IPC_SEM_EMPTY_NAME);
	sem_open(IPC_SEM_EMPTY_NAME, /* name */
		 O_CREAT,       /* create the semaphore */
		 ACCESS_PERMS,   /* protection perms */
		 Q_SIZE);    /* All cells are empty */
	
	/* Open Shared Mem file */
	fd = shm_open(IPC_SHARED_MEM_FILE, O_CREAT | O_RDWR, ACCESS_PERMS);  /* empty to begin */
	if (fd < 0)
		report_and_exit("Can't get file descriptor...");
	/* Determine Shared Memory size */
	ftruncate(fd, SOLVER_SHMEM_SIZE);

	/* Map the kernel's Shared Memory to the current process virtual memory*/
	solver_shmem_ptr = mmap(NULL,
				SOLVER_SHMEM_SIZE,   /* how many bytes */
				PROT_READ | PROT_WRITE, /* access protections */
				MAP_SHARED, /* mapping visible to other processes */
				fd,         /* file descriptor */
				0);         /* offset: start at 1st byte */
	if ((struct solverShmem *)-1  == solver_shmem_ptr)
		report_and_exit("Can't get segment...");

	solver_shmem_ptr->prod = 0;
	solver_shmem_ptr->cons = 0;

	/* Fork the Python ipc.py. Note that the py_pid, is not the read pid of the ipc.py,
	 * since the "system" command spawn a different process. As result, the python PID is
	 * passed to the producer process using the pipe 
	 */
	py_pid = fork();
	if (py_pid == 0) {
		system("python ipc.py");
		exit(0);
	}
	num_child_processes++;
	/* Send the list of files to the Python ipc via socket */
	send_list_of_files(files, num_files);
	
	/* Fork producers and consumers */
	pid = fork();
	if (pid == 0) {
		prod_main(num_files);
		exit(0);
	}
	num_child_processes++;
	pid = fork();
	if (pid == 0) {
		cons_main();
		exit(0);
	}
	num_child_processes++;
	/* Wait function waits for an arbitrary child process to complete.
	 * In our case, we want all child processes to complete before unlinking the shared memory,
	 * so it waits for 3 processes
	 */
	while (num_child_processes) {
		wait(NULL);
		num_child_processes--;
	}
	printf("Main process finished\n");
	close(fd);
	unlink(IPC_SHARED_MEM_FILE);
}
