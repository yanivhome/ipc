import socket
import sys
import os
import time
import signal

def signal_handler(signum, frame):  
    print >> sys.stdout, 'SIGUSR1 signal received signum %s frame %s'%(signum, frame)

signal.signal(signal.SIGUSR1, signal_handler)

server_address = '/ipcSocket'

# Make sure the socket does not already exist

try:
    os.unlink(server_address)
except OSError:
    print >> sys.stdout, 'unlink failed'
##    if os.path.exists(server_address):
##        raise

# Create a UDS socket
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

# Bind the socket to the port
print >>sys.stdout, 'starting up on %s' % server_address
sock.bind(server_address)

# Listen for incoming connections
sock.listen(1)

## PIPE for sending data to C
PIPE_NAME = "/ipcPipe"
  
# Mode of the FIFO (a named pipe)
# to be created
mode = 0o644
  
# Create a FIFO named path
# with the specified mode
# using os.mkfifo() method

try:
    os.unlink(PIPE_NAME)
except:
    print >> sys.stdout, 'unlink PIPE_NAME failed'

os.mkfifo(PIPE_NAME)
#pipe = os.open(PIPE_NAME, os.O_WRONLY)
pipe = os.open(PIPE_NAME, os.O_RDWR)
#pipe = os.open(PIPE_NAME, os.O_NONBLOCK)

print ('pipe opened')

# Wait for a connection
print >>sys.stdout, 'waiting for a connection'
connection, client_address = sock.accept()
try:
    print >>sys.stdout, 'connection from', client_address
    data = connection.recv(1024)
    print >>sys.stdout, 'Got list of files ' + data
    files = data.split(';')
    os.write(pipe, str(os.getpid()) +'\n');
    time.sleep(5)
    for file in files:
        try:
            f = open(file, "rt");
        except:
            print >>sys.stdout, 'Failed reading file %s' %file
            continue;
        for line in f:
            os.write(pipe, line);

        print >>sys.stdout, 'Wait on signal' 
        os.close(pipe);
       
        signal.pause()
        print >>sys.stdout, 'Got signal. Re-opening pipe'
        pipe = os.open(PIPE_NAME, os.O_RDWR)
        #signal.sigwait(SIGUSR1)
finally:
    # Clean up the connection
    connection.close()
        
