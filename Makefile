server_name ?= "SC-Server"
client_name ?= "SC-Client"

CLIENT_DIR=./src/Client/
SERVER_DIR=./src/Server/

CFLAGS ?= -O3

all: client server 

client:
	$(MAKE) -C $(CLIENT_DIR) PROG_NAME=$(client_name)
	@mv $(CLIENT_DIR)/$(client_name) ./

server:
	$(MAKE) -C $(SERVER_DIR) PROG_NAME=$(server_name)
	@mv $(SERVER_DIR)/$(server_name) ./

.PHONY: clean
clean:
	$(MAKE) -C $(SERVER_DIR) clean
	$(MAKE) -C $(CLIENT_DIR) clean
