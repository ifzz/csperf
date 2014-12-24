csperf
======

csperf: Client Server Performance measurement tool. 

Summary
-------

CSPerf actively measures the bandwith, latency and RTT of the network 
while transmitting TCP traffic.

Client sends a special MARK command before the data transfer 
which informs the server what needs to be done with the data.
Once the transfer is completed, server responds with a MARK_RESP
command which allows us to calculate the time taken by the sever to 
process the data.

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
csperf [-s|-c host] [options]

-c <hostname>         # Run as client and connect to hostname
-s                    # Run as server
-p <port>             # Server port to list to. Default 5001
-B <data block size>  # Size of the data segment. Default 1KB
-n <num blcks>        # Number of data blocks to send. Default 1000
-e                    # Echo client data. Server echos client data

Bug Reports
-----------
https://github.com/niks3089/csperf/issues
