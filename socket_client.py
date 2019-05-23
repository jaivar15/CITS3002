"""
This is a simple example of a client program written in Python.
Again, this is a very basic example to complement the 'basic_server.c' example.


When testing, start by initiating a connection with the server by sending the "init" message outlined in 
the specification document. Then, wait for the server to send you a message saying the game has begun. 

Once this message has been read, plan out a couple of turns on paper and hard-code these messages to
and from the server (i.e. play a few rounds of the 'dice game' where you know what the right and wrong 
dice rolls are). You will be able to edit this trivially later on; it is often easier to debug the code
if you know exactly what your expected values are. 

From this, you should be able to bootstrap message-parsing to and from the server whilst making it easy to debug.
Then, start to add functions in the server code that actually 'run' the game in the background. 
"""

import socket, sys, select
from time import sleep
# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect the socket to the port where the server is listening
server_address = ('localhost', 4445)
print ('connecting to %s port %s' % server_address)
sock.connect(server_address)

class TimeoutExpired(Exception):
    pass

def input_with_timeout(prompt, timeout):
    sys.stdout.write(prompt)
    sys.stdout.flush()
    ready, _, _ = select.select([sys.stdin], [],[], timeout)
    if ready:
        return sys.stdin.readline().rstrip('\n') # expect stdin to be line-buffered
    raise TimeoutExpired

count=0
try:
    message = 'INIT'.encode()
    print ('sending "%s"' % message)
    sock.sendall(message)
    while True:
    
        exit = False
        id = ""
        while True:
            data = sock.recv(1024)
            sleep(3)
            mess = data.decode()
            if "WELCOME" in mess:
                print ( 'received "%s"' % mess)
                id = mess[8:11]
                continue
            print ( 'received "%s"' % mess)
            sleep(2)
            if "VICT" in mess:
                print("you win")
                exit = True
            if "ELIM" in mess:
                print("you lose")
                exit = True
            if exit:
                break
            prompt = "please make a move"
            try:
                answer = input_with_timeout(prompt, 18)
            except TimeoutExpired:
                print('Sorry, times up')
            else:
                sock.sendall((id+",MOV,"+answer).encode())
                print('Got %r' % answer)
        if exit:
            break
finally:    
    print ('closing socket')
    sock.close()
