# Project- System Control
This project lets you connect remotly to a device in the same local network automatically (using multicast) or with a given IP address and control it with bash. The remote device has to be running the SC-Server program so that a SC-Client program can connect to it and start exchanging data. The SC-Server creates a pseaudo-terminal on which runs bash and forwards user sent input (given it's a command client request). Everything read from the terminal (e.g. bash output from a command) is send back to the client and rendered via the client's terminal line discipline (or if running in ncurses, via virtual terminal emulator, i.e. libvterm).

https://github.com/user-attachments/assets/fc9f0d3a-ad26-44dc-b6d4-0099fa4d5445

https://github.com/user-attachments/assets/3b21a184-b29a-4796-9ba3-693ff2e7595c

