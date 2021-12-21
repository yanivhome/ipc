#define IPC_SHARED_MEM_FILE "/ipcShareMem"
#define IPC_SEM_FULL_NAME "/ipcSemFull"
#define IPC_SEM_EMPTY_NAME "/ipcSemEmpty"
#define IPC_SOCKET_NAME "/ipcSocket"
#define IPC_PIPE_NAME "/ipcPipe"
#define ACCESS_PERMS 0666
#define MAX_LINE_LEN 256
// 0664
// 6 (read and write) for the owner
// 4 (readonly) for other group users
// 4 (readonly) for anyone else
#define Q_SIZE 10
enum oper {PLUS, MIN, MUL, DIV, TERMINATE_OP=-1};
struct Question{
      int num1;
      int num2;
      enum oper   op;
};
struct solverShmem {
	struct Question array[Q_SIZE];
	int prod;
	int cons;
};
/*
 * * Each thread sleep for n seconds
 * * Generates 2 random numbers between lowval and highval
 * * Generates random operator
 * * Stores the question in the questions array
 * * Create both threads with the same function (send n, lowval, highval to the thread function)
 * */
//struct Question array[Q_SIZE];
#define SOLVER_SHMEM_SIZE (sizeof(struct solverShmem))
void report_and_exit(const char* msg);
int prod_main(int);
int cons_main(void);
