# SepticManager
managing interaction with septic automation

//**********************************************************************
for using jansson.h not forget install jansson library. 
e.i. Dowload jansson project, and install. For futher info read README in janson library. 
And not forget add -ljansson flag in project build properties in linker other options
//**********************************************************************
for succesful compiling of sqlite3 add link libraries: 
pthread
dl
to project build properties ->link libraries
//**********************************************************************
$ netcat -U /tmp/septic_socket.socket
request from web client to this:

{"tag": 1/2/3 (TAG_REQSNAP/TAG_CMDRESETALARM/TAG_DB_REQUEST****}

if tag==TAG_REQSNAP there is explition parameter of requested data:
"val": "act_alarm"/"pend_alarm"/"msg_timer"}
i.e. {"tag": 1, "val": "msg_timer"}
{"tag": 1, "val": "pend_alarm"}
{"tag": 1, "val": "act_alarm"}

//**********************************************************************
cath this messages (activ/pending):
01 0a 00 0a c0 e5 02 c3 00 01 00 00
01 0a 00 0a c0 e5 02 c4 00 01 00 00

you can use bellow, but better use python prog to periodically send messages
echo -n -e "\x01\x0a\x00\x0a\xc0\xe5\x02\xc3\x00\x01\x00\x00" | nc -lU  /tmp/uart_socket.socket
python example:
****
import socket
import os
import time

# Set the path for the Unix socket
socket_path = '/tmp/uart_socket.socket'

# remove the socket file if it already exists
try:
    os.unlink(socket_path)
except OSError:
    if os.path.exists(socket_path):
        raise

# Create the Unix socket server
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

# Bind the socket to the path
server.bind(socket_path)

# Listen for incoming connections
server.listen(1)

# accept connections
print('Server is listening for incoming connections...')
connection, client_address = server.accept()

try:
    print('Connection from', str(connection).split(", ")[0][-4:])

    # receive data from the client
    mes_cnt=0
    while True:
        time.sleep(7)
        mes_cnt=mes_cnt+1
        # Send a response back to the client
        response = 'Hello from the server!'
        if mes_cnt<5:
            connection.sendall(bytes.fromhex('01 0a 00 0a c0 e5 02 c4 00 01 00 00'))
        else:
            connection.sendall(bytes.fromhex('01 0a 00 0a c0 e5 02 c4 00 01 00 01'))
            mes_cnt=0
finally:
    # close the connection
    connection.close()
    # remove the socket file
    os.unlink(socket_path)

***
