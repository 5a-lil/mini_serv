# mini_serv.c

Small C server that permits you to send messages to all other connected clients using as base for I/O (Input/Ouput) the function **select()** from **sys/select.h**. 

As a connected client every messages sent are sended to all other connected clients with your **client id** in front of the messages. You stop the server with ctrl-C.

Handles for clients:
- Connection
- Disconnection
- Global messages

To run the server do:
```
$ cc mini_serv.c -o serv
```
then:
```
$ ./serv
```

Have fun with it ! 🔥
