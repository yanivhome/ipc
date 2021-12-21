/** Compilation: gcc -o memwriter memwriter.c -lrt -lpthread **/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <signal.h>
#include "shmem.h"

struct prod_info {
	int *p;
	FILE *pipe_fd;
	int py_pid;
	struct solverShmem *memptr;

};

static enum oper c2op(char c)
{
	switch (c) {
	case '+':
		return PLUS;
	case '-':
		return MIN;
	case '*':
		return MUL;
	case '/':
		return DIV;
	}
	return -1;
}
static int notify_consumer(struct prod_info *info)
{
	struct solverShmem *shmem = info->memptr;
	int q_prod = shmem->prod;

	printf("Signalling consumer to stop\n");
	shmem->array[q_prod].op = TERMINATE_OP;
	q_prod = (q_prod + 1) % Q_SIZE;
	shmem->prod = q_prod;
	return 0;
}
static int producer(struct prod_info *info)
{
	struct solverShmem *shmem = info->memptr;
	char line[MAX_LINE_LEN];
	char c;
	int q_prod = shmem->prod;
	int q_cons = shmem->cons;

	static int killed = 0;
	
	if (fgets(line, MAX_LINE_LEN, (FILE *)info->pipe_fd) == NULL) {
		// NULL is received only if the Write pipe-end is closed (or in case of error)
		if (!killed) {
			printf("EOF. Signalling python to analyze another file pid %d\n", info->py_pid);
			kill(info->py_pid, SIGUSR1);
			killed = 1;
		}
		return -1;
	}
	killed = 0;
	sscanf(line, "%d %c %d", &shmem->array[q_prod].num1, &c, &shmem->array[q_prod].num2);
	shmem->array[q_prod].op = c2op(c);

	printf("producer: Producer# %d Consumer# %d\n", q_prod, q_cons);
	printf("Producer: num1:%d Op:%d Num2:%d\n", shmem->array[q_prod].num1, shmem->array[q_prod].op, shmem->array[q_prod].num2);
	q_prod = (q_prod + 1) % Q_SIZE;
	shmem->prod = q_prod;
	return 0;
}

int prod_main(int num_files)
{
	struct prod_info info;
	struct solverShmem *solver_shmem_ptr;
	int val;
	char line[MAX_LINE_LEN];
	char *myfifo = IPC_PIPE_NAME;
	sem_t * mutex_sem,*full_sem,*empty_sem;

	/* Open shared mempory file */
	int shm_fd = shm_open(IPC_SHARED_MEM_FILE, /* name from smem.h */
			      O_RDWR,              /* read/write, create if needed */
			      ACCESS_PERMS);       /* access permissions (0644) */
	if (shm_fd < 0)
		report_and_exit("Can't open shared mem segment...");

	/* Map the shared memory file created by kernel to the process's virtual address space */
	solver_shmem_ptr = mmap(NULL,                   /* let system pick where to put segment */
				SOLVER_SHMEM_SIZE,      /* how many bytes */
				PROT_READ | PROT_WRITE, /* access protections */
				MAP_SHARED,             /* mapping visible to other processes */
				shm_fd,                 /* file descriptor */
				0);                     /* offset: start at 1st byte */
	if ((struct solverShmem *)-1  == solver_shmem_ptr)
		report_and_exit("Can't get segment...");

	fprintf(stderr, "shared mem address: %p [0..%d]\n", solver_shmem_ptr, SOLVER_SHMEM_SIZE - 1);
	fprintf(stderr, "Shared mem file:     /dev/shm%s\n", IPC_SHARED_MEM_FILE);
	info.memptr = solver_shmem_ptr;
	/* Open two sempahores (They were created in the main process):
	 * empty_sem - contains the number of empty cells in the ring.
	 *           - Producer works only while empty_sem > 0 (wait), and decrease by 1.
	 *           - After consumer consumes, it increment it
	 * full_sem  - contains the number of populated cells. 
	 *           - Consumer works only while full_sem > 0 (otherwise there's nothing to consume)
	 *           - After producer populates new cell, it increment it.
	 */
	empty_sem = sem_open(IPC_SEM_EMPTY_NAME, /* name */
			     O_RDWR,             /* create the semaphore */
			     ACCESS_PERMS,       /* protection perms */
			     0);                 /* initial value (cannot be changed after creation) */

	if (empty_sem == (void *)-1)
		report_and_exit("sem_open");

	full_sem = sem_open(IPC_SEM_FULL_NAME, /* name */
			    O_RDWR,            /* create the semaphore */
			    ACCESS_PERMS,      /* protection perms */
			    0);                /* initial value (cannot be changed after creation)*/

	if (full_sem == (void *)-1)
		report_and_exit("sem_open"); 

	/* Create pipe from python */
	mkfifo(myfifo, 0466);
	info.pipe_fd = fopen(myfifo, "r");
	/* Acquire the python pid via the pipe */
	if (fgets(line, MAX_LINE_LEN, (FILE *)info.pipe_fd) == NULL) {
		printf("Failed opening pipe\n");
		return 0;
	}
	info.py_pid = atoi(line);

	while (num_files) {
		sem_wait(empty_sem);
		if (producer(&info)) {
			sem_post(empty_sem);
			num_files--;
			continue;
		}
		sem_post(full_sem);
	}
	printf("Producer terminates\n");
	notify_consumer(&info);
	/* Need to use semaphore to nofify customer that there's a new data */
	sem_post(full_sem); 
	/* clean up */
	munmap(IPC_SHARED_MEM_FILE, SOLVER_SHMEM_SIZE); /* unmap the storage */
	close(shm_fd);
	sem_close(full_sem);
	sem_close(empty_sem);
	shm_unlink(IPC_SHARED_MEM_FILE); /* unlink from the backing file */

	/* Kill the python file reader process */
	kill(info.py_pid, SIGKILL);
	return 0;
}
