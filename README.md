# Project: System Control
This project lets you connect remotly to a device in the same local network automatically (using multicast) or with a given IP address and control it with bash. The remote device has to be running the SC-Server program so that a SC-Client program can connect to it and start exchanging data. The SC-Server creates a pseaudo-terminal on which runs bash and forwards user sent input (given it's a command client request). Everything read from the terminal (e.g. bash output from a command) is send back to the client and rendered via the client's terminal line discipline (or if running in ncurses, via virtual terminal emulator, i.e. libvterm).

There is a .service file for the SC-Server, for the automatic start of the program by systemd at boot.

If the server is started with root privileges then it uses "/var/lib/SC-Server" for storage directory (e.g. for storing source and object kernel module files). Else it uses the "/home/{user}/SC-Server" directory.

The names of the programs can easily be changed through the Makefile.

## Options
### SC-Server 
* **--daemoned**\
    Runs the server in daemoned mode (defined by Unix).
  
### SC-Client
* **-a, --address**\
    Doesn't begin to search for multicast messages in the local network, but uses the given IP address to search for the device that runs the SC-Server. Error message if the given address is invalid.
* **-f, --files** list of file names\
    Sends the given files to the server, which writes them on the device.
* **-d, --drivers** list of *.c files\
    Like in the "-f" option, but the server also compiles them as a Linux Kernel Module agains the linux kernel on it's the device and installs them on the running OS (given it has super user privileges).
* **-p, --program** [program name]\
    Only works if the SC-Client is compiled without specifiing the -DWITHOUT_NCURSES flag to the compiler and also if the "-n" flag is **not** given to the program.\
    If no argument is given to this option then the default program (dmesg --follow -H) isn't started.\
    If a valid program name is given, the server starts the program and sends the output to the client. The client renders the output in another window, also writing it to the stderr stream.
* **-n, --no-ncurses**\
    The SC-Client program functions just as a basic terminal. (way less buggy)

## Building
### Dependencies
This project is build on Linux and it uses make. It requires a working C compiler, standard library and headers.\
If building with ncurses (graphical interface) then **_ncurses 6.5+_** and **_libvterm 0.3+_** must also be installed.

### Steps
1.  **Clone the repository:**
    ```bash
    git clone https://github.com/kristianvaleriev/Project-System-Control/
    cd Project-System-Control
    ```
2.  **Make  the programs:**
    ```bash
    make // builds both SC-Server and SC-Client;
    
    make client  [client_name=<name>]
    make serrver [server_name=<name>]
    ```
3. **Delete/Clear object files and dependencies:**
    ```bash
    make clean
    ```
    
## Demo videos
https://github.com/user-attachments/assets/fc9f0d3a-ad26-44dc-b6d4-0099fa4d5445

https://github.com/user-attachments/assets/bf1bd328-2ace-4b6c-8332-d4629708c1ea

