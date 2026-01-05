# Project: System Control
This project lets you connect remotly to a device in the same local network automatically (using multicast) or with a given IP address and control it with bash. The remote device has to be running the SC-Server program so that a SC-Client program can connect to it and start exchanging data. The SC-Server creates a pseaudo-terminal on which runs bash and forwards user sent input (given it's a command client request). Everything read from the terminal (e.g. bash output from a command) is send back to the client and rendered via the client's terminal line discipline (or if running in ncurses, via virtual terminal emulator, i.e. libvterm).

## Options
### SC-Server
* **--daemoned**
    Runs the server in daemoned mode (defined by Unix). Uses syslog() for debug messages and information.
  
### SC-Client
* **-a, --address**
    Doesn't begin to search for multicast messages in the local network, but uses the given IP address to search for the device that runs the SC-Server. Error        message if the given addr is invalid.
  
## Demo videos
https://github.com/user-attachments/assets/fc9f0d3a-ad26-44dc-b6d4-0099fa4d5445

https://github.com/user-attachments/assets/bf1bd328-2ace-4b6c-8332-d4629708c1ea

