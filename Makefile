
CC = gcc
CFLAGS=-Wall
#LDFLAGS=-lpthread


SRC_CLIENT = client.c 
SRC_SERVER = server.c


EXEC_CLIENT = client
EXEC_SERVER = server

all:
	@echo "Création des exécutables client et server." 
	$(CC) $(CFLAGS) $(SRC_CLIENT) -o $(EXEC_CLIENT)
	$(CC) $(CFLAGS) $(SRC_SERVER) -o $(EXEC_SERVER)
	
client:
	@echo "Création de l'éxecutable client."
	$(CC) $(CFLAGS) $(SRC_CLIENT) -o $(EXEC_CLIENT)
	

server:
	@echo "Création de l'éxecutable server."
	$(CC) $(CFLAGS) $(SRC_SERVER) -o $(EXEC_SERVER)


clean:
	rm -f client server

