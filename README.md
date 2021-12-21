#IPC
#By:Yaniv
#21-Dec-21
###### This program demonstrates various IPC techniques interleaved together. It is compound out of 4 processes which work together to calculate given math exersizes from input files, like 5 * 10. The processes are: main, producer, consumer and ipc (python - just for fun)
###### To compile: gcc -o ipc main.c producer.c consumer.c -lpthread -lrt
###### Run example: ./ipc -f data_files/file1.txt -f data_files/file2.txt -f data_files/file3.txt
## The main process
###### Launch first and accept file names as input
###### 1. It creates 2 **semaphores** - empty and full, that will serve as syncronization means 
######    between the producer and consumer processes
###### 2. It creates **SharedMemory** and  maps (**mmap()**) it to the the process's virtual address
######    space in order to initialize the producer and consumer of the shared memory ring.
###### 3. It forks the python ipc.py process first, and send it the list of files given as input
######    over a **socket()**. Note that this named socket has two parties, where one needs first
######    to **accept()** connection before the other side initiate a **connect()** followed by 
######    sending the file names. Since the python ipc.py is the one that needs to accept the
######    connection, the main process **sleep()** for a second after forking it, and before **connect()**
######    The python process is responsible for reading these files one by one and send each line 
######    to the producer. The python will wait for the producer **signal** before
######    moving on the next file.
######    Note: Forking the python process is done using **system()** command, but unlike regular forking,
######    the **system()** command forks the python again, so the pid returned from the **fork()** is not
######    the actual pid of the ipc.py process. As a result, the python process will be forced to send its
######    own pid to the producer process via the **pipe**
###### 4. Fork the producer and consumer processes (**fork()**)
###### 5. Wait for all processes to terminate (**wait()**)
###### 6. close and unlink (*unlink()*) the SharedMemory to free up this resource
#
## The producer-consumer are synced using two **semaphores** - empty and full. 
###### The empty semaphore is initialized as the ring size since that's the number of vacant places
###### The full semaphore is initialized as 0.
###### Note that the semaphore init size can be determines only once, so it is done in the main process.
### producer:                                             
###### wait(empty) - In case empty is non-zero, it decrement the empty by 1, and proceed to the processing part
###### process()   - place new data in the ring and increment the producer 
###### post(full)  - Increment the full semaphore by 1, letting the consumer know there's additional data in the ring.
###
### consumer:
###### wait(full)  - In case full is non-zero, decrement it by 1, and proceed to consume the data.
###### process()   - consume the data in the cons location
###### post(empty) - Once data was consumed, can increment the empty semaphore letting the producer know there's one more vacant spot.
##
## The producer process
###### First, it opens the SharedMemory file, and map it to the process's virtual address space.
###### Next, it opens the empty and full semaphores needed to sync the ring processing
###### This process communicates both with the consumer process, and with the ipc.py python process
###### To receive the raw data from the python process, it opens a named pipe, which is also called FIFO (**mkfifo()**).
###### Next it wait for the Python process to send its own PID via this pipe. This is required later for the producer to **signal()**
###### the Python process to process the next file.
###### The main loop focus on placing new data in the ring while the empty **semaphore** allows it, and update the full **semaphore** (See producer-consumer above)
###### When the Python process finish processing a file, it closes the pipe towards the producer process, which produce NULL for **fgets()**.
###### As a result, the producer send **signal()** to the Python process to process the next file, and the latter re-open the **pipe**
###### When all files have been processed, the producer wants to notify the consumer process about it. 
###### For that, it sends a predefined non-math operation, and free up resources. The Consumer process, in turn, intercept this message,
###### and knows to exit the processing loop.


