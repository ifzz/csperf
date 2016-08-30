csperf
======

csperf: Performance measurement tool which helps measuring the network capabilities. 

Summary
-------

CSPerf actively measures the latency and RTT of the network while transmitting TCP traffic.

Client sends a special MARK command before the data transfer which informs the server what needs to be done with the data.
Once the transfer is completed, server responds with a MARK_RESP command which allows us to calculate the time taken by the sever to process the data.

It also allows multiple clients to connect to the server at desired concurrency and at different rates.

Libevent and Bufferevent is used to set up sockets and send/receive data.

Obtaining Csperf 
----------------
git clone https://github.com/niks3089/csperf.git

Building Csperf
----------------
### Prerequisites: ###
CMAKE (Version > 2.8)

### Building ###
make; sudo make install

Using Csperf
-------------
```
csperf [-s|-c host] [options]

 -c <hostname>         # Run as client and connect to hostname
 -s                    # Run as server
 -p <port>             # Server port to list to. Default 5001
 -B <data block size>  # Size of the data segment. Default 1KB
 -n <num blcks>        # Number of data blocks to send. Default 1
 -e                    # Echo client data. Server echos client data
 -C <num-clients>      # Total number of clients
 -P <num-clients>      # Concurrent/Parallel clients that needs to connect to the server
 -S <num-clients>      # Number of Clients that connects with the server every second
 -r <repeat count>     # Repeat the test these many times. Setting -1 means run forever
 -l <logfile>          # Logfile to write to. Default writes to csperf_xxx.txt xxx = client or server
 
```

Bug Reports
-----------
https://github.com/niks3089/csperf/issues
