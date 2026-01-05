#Project- System Control
This project lets you connect remotly to a device in the same local network automatically (using multicast) or with a given IP address and control it with bash. The remote device has to be running the SC-Server program so that a SC-Client program can connect to it and start exchanging data. The SC-Server creates a pseaudo-terminal on which runs bash and forwards user sent input (given it's a command client request). Everything read from the terminal (e.g. bash output from a command) is send back to the client and rendered via the client's terminal line discipline (or if running in ncurses, via virtual terminal emulator, i.e. libvterm).

https://github.com/user-attachments/assets/0d84c24d-aa43-4b77-8337-e782fbe178ad

https://github.com/user-attachments/assets/bf96ceeb-c518-4833-a46c-6c88bd123b84

