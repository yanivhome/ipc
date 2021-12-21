/** Compilation: gcc -o memreader memreader.c -lrt -lpthread **/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include "shmem.h"

static int consumer(struct solverShmem *shmem)
{
	char str[100];
	int q_cons = shmem->cons;
	int q_prod = shmem->prod;
	struct Question *q;

	printf("consumer: Producer %d Consumer %d\n", q_prod, q_cons);
	q = &shmem->array[q_cons];
	switch (q->op) {
	case PLUS:
		sprintf(str, "Result of %d + %d = %d\n", q->num1, q->num2, q->num1 + q->num2);
		break;
	case MIN:
		sprintf(str, "Result of %d - %d = %d\n", q->num1, q->num2, q->num1 - q->num2);
		break;
	case MUL:
		sprintf(str, "Result of %d * %d = %d\n", q->num1, q->num2, q->num1 * q->num2);
		break;
	case DIV:
		if (q->num2 == 0)
			sprintf(str, "Cannot divide by zero\n");
		else
			sprintf(str, "Result of %d / %d = %d\n", q->num1, q->num2, q->num1 / q->num2);
		break;
	default:
		if (q->op != TERMINATE_OP) { 
			printf("Error: unknown op %d\n", q->op);
		}
		return 1; /* End */
	}
	q_cons = (q_cons + 1) % Q_SIZE;
	puts(str);
	shmem->cons = q_cons;
	return 0;
}

int cons_main()
{
	int val;
	int end = 0;
	sem_t * full_sem,*empty_sem;
	struct solverShmem *solver_shmem_ptr;
	int shm_fd = shm_open(IPC_SHARED_MEM_FILE, /* name from smem.h */
			      O_RDWR,              /* read/write, create if needed */
			      ACCESS_PERMS);       /* access permissions (0644) */

	if (shm_fd < 0)
		report_and_exit("Can't get file descriptor...");

	/* get a pointer to memory */
	solver_shmem_ptr = mmap(NULL,                   /* let system pick where to put segment */
				SOLVER_SHMEM_SIZE,      /* how many bytes */
				PROT_READ | PROT_WRITE, /* access protections */
				MAP_SHARED,             /* mapping visible to other processes */
				shm_fd,                 /* file descriptor */
				0);                     /* offset: start at 1st byte */
	if ((struct solverShmem *)-1 == solver_shmem_ptr)
		report_and_exit("Can't access segment...");

	empty_sem = sem_open(IPC_SEM_EMPTY_NAME, /* name */
			     O_RDWR,             /* create the semaphore */
			     ACCESS_PERMS,       /* protection perms */
			     0);                 /* initial value */

	if (empty_sem == (void *)-1)
		report_and_exit("sem_open");

	full_sem = sem_open(IPC_SEM_FULL_NAME, /* name */
			    O_RDWR,            /* create the semaphore */
			    ACCESS_PERMS,      /* protection perms */
			    0);                /* initial value */

	if (full_sem == (void *)-1)
		report_and_exit("sem_open");
	

	/* use semaphore as a mutex (lock) by waiting for writer to increment it */
	while (!end) {
		sem_wait(full_sem);
		end = consumer(solver_shmem_ptr);
		sem_post(empty_sem);
	}
	printf("Consumer terminates\n");
	/* cleanup */
	munmap(IPC_SHARED_MEM_FILE, SOLVER_SHMEM_SIZE); /* unmap the storage */
	close(shm_fd);
	sem_close(full_sem);
	sem_close(empty_sem);
	shm_unlink(IPC_SHARED_MEM_FILE); /* unlink from the backing file */
	return 0;
}
