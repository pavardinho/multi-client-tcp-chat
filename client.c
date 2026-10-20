#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <ctype.h>
#include <time.h>


#include "common.h"
#include "msg_struct.h"

void die(int status, char* str) {
    if (status == -1) {
        perror(str);
        exit(EXIT_FAILURE);
    }
}

int read_message(int sockfd, struct message *msg, char *buff) {

	int to_send = sizeof(struct message);
    int sent = 0;
    int ret;
    
	while (to_send != sent) {
        ret = read(sockfd, ((char *)msg) + sent, to_send - sent);
        if (ret <= 0){
            return ret;
        }
        sent += ret;
    }
   
    to_send = msg->pld_len;	
    sent = 0;

    if (to_send == 0) {
        buff[0] = '\0';
        return 1;
    }

    while (to_send != sent) {
    	ret = read(sockfd, buff + sent, to_send - sent);
        if (ret <= 0){
            return ret;
        }
        sent += ret;
    }

    return 1;
}

void write_message(int sockfd, struct message *msg, const char *buff) {

	int to_send = sizeof(struct message);
    int sent = 0;
    int ret;
    
	while (to_send != sent) {
        ret = write(sockfd, ((char *)msg) + sent, to_send - sent);
        die(ret, "on write struct");
        sent += ret;
    }

    
    to_send = msg->pld_len;	
    sent = 0;
    while (to_send != sent) {
    	ret = write(sockfd, buff + sent, to_send - sent);
        die(ret, "on write buff");
        sent += ret;
    }
}

void echo_client(int sockfd) {
	struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    char buff[MSG_LEN];

	static char my_nick[NICK_LEN] = "";
	char* message;

	int is_msg = 0;
	
	while (1) {

		int ret = poll(fds, 2, -1);
        die(ret, "poll");

		// lié au clavier
        if (fds[0].revents & POLLIN) {

        	// Cleaning memory
			memset(buff, 0, MSG_LEN);
			// Getting message from client
			int n = 0;
			char c;
			while ((c = getchar()) != '\n' && n < MSG_LEN - 1) {
			    buff[n++] = c;
			}
			buff[n] = '\0';

            // commande /quit
            if (strcmp(buff, "/quit") == 0) {
                printf("Déconnexion...\n");
                close(sockfd);
                exit(0);
            }

            struct message msg;
            memset(&msg, 0, sizeof(msg));

			if (my_nick[0] == '\0' && strncmp(buff, "/nick", 5) != 0) {
                printf("[Server] : please login with /nick <your pseudo>\n");
                continue;
            }

            // Si commande /nick
            if (strncmp(buff, "/nick", 5) == 0) {
				
				char *pseudo = buff + 5;
                while (*pseudo == ' ') {
                    pseudo++;
                }

				if (*pseudo == '\0') {
                    printf("Error : use of /nick : /nick <your_pseudo>\n");
                    continue;
                }

				int check = 1;
				if (strlen(pseudo) >= NICK_LEN) {
					check = 0;
				}
				for (int i=0; i<strlen(pseudo); i++){
					if (!isalnum(pseudo[i]) && !isalpha(pseudo[i])){
						check = 0;
						break;
					}
				}
				if (check == 0){
					printf("Error: Nickname invalid (max %d chars, alphanumeric only)\n", NICK_LEN-1);
					continue;
				}
				
				msg.type = NICKNAME_NEW;
                msg.nick_sender[0] = '\0';
                strncpy(msg.infos, pseudo, NICK_LEN-1);
                msg.infos[NICK_LEN-1] = '\0';
                msg.pld_len = 0;
				msg.dest[0] = '\0';

			} 

			// Si commande /who
			else if (strcmp(buff, "/who") == 0) { 
                msg.type = NICKNAME_LIST;

                strncpy(msg.nick_sender, my_nick, NICK_LEN-1);
                msg.pld_len = 0;
				msg.dest[0] = '\0';
			
			} 

			// Si commande /whois
			else if (strncmp(buff, "/whois", 6) == 0) {
                
                char *target_nick = buff + 6;
                while (*target_nick == ' ') {
                    target_nick++;
                }

                if (*target_nick == '\0') {
                    printf("Error : use of /whois : /whois <target_pseudo>\n");
                    continue;
                }

                msg.type = NICKNAME_INFOS;
                strncpy(msg.infos, target_nick, INFOS_LEN-1);
                msg.infos[INFOS_LEN-1] = '\0';

                strncpy(msg.nick_sender, my_nick, NICK_LEN-1);
                msg.pld_len = 0;
				msg.dest[0] = '\0';

            }

			
			//si c'est une commande /msgall
			else if(strncmp(buff, "/msgall", 7) == 0) {
				message = buff + 7;
				while (*message == ' ') {
					message++;
				}
				if (*message == '\0') {
					printf("[SERVER] Put a message after the /msgall : \t /msgall *message* \n");
					continue;
				}
				msg.type = BROADCAST_SEND;

				strncpy(msg.nick_sender, my_nick, NICK_LEN-1);
				msg.pld_len = strlen(message);
				msg.dest[0] = '\0';
				is_msg = 1;
			}

			//si c'est une commande /msg 
			else if (strncmp(buff, "/msg", 4) == 0) {
				message = buff +4;
				while (*message == ' ') {
					message++;		
				}
				if (*message == '\0') {
					printf("[SERVER] Put a Nickname after the /msg : \t /msg [Nickname] *message* \n");
					continue;
				}
				char* user_vise = malloc(NICK_LEN * sizeof(char));
				for(int i = 0; i < NICK_LEN - 1 && *message != ' ' && *message != '\0'; i++, message++) {
					user_vise[i] = *message;
					user_vise[i + 1] = '\0';
				}
				msg.type = UNICAST_SEND;
				strcpy(msg.dest, user_vise);
				while (*message == ' ') {
					message++;
				}
				if (*message == '\0') {
					printf("[SERVER] Put a message after the Nickname: : \t /msg [Nickname] *message* \n");
					free(user_vise);
					continue;
				}

				strncpy(msg.nick_sender, my_nick, NICK_LEN-1);
				msg.pld_len = strlen(message);
				free(user_vise);
				is_msg = 1;
			}

			// Si besoin d'aide
			else if (strcmp(buff, "/help") == 0) {
				printf("Available commands:\n");
				printf("\t -/nick <your_pseudo> : Set or change your nickname (alphanumeric only, max %d chars)\n\n", NICK_LEN-1);
				printf("\t -/who : List all online users\n\n");
				printf("\t -/whois <target_pseudo> : Get information about a specific user\n\n");
				printf("\t -/msgall <message> : Send a message to all users\n\n");
				printf("\t -/msg <nickname> <message> : Send a private message to a specific user\n\n");
				printf("\t -/quit : Disconnect from the server\n\n");
				printf("\t -/help : Show this help message\n\n");
				continue;
			}

			// Sinon message echo
			else {
				msg.type = ECHO_SEND;
				
				strncpy(msg.nick_sender, my_nick, NICK_LEN-1);
				msg.pld_len = strlen(buff);
				msg.dest[0] = '\0';
			}
		

			if(msg.type == BROADCAST_SEND || msg.type == UNICAST_SEND){
				write_message(sockfd, &msg, message);
			}

			else{
            	write_message(sockfd, &msg, buff);
        	}

			if(is_msg == 1){
				printf("Message sent. \n");
			}
		}

		// reception d'un message server
		if (fds[1].revents & POLLIN) {
            struct message msg;
            memset(&msg, 0, sizeof(msg));
            memset(buff, 0, MSG_LEN); 

            int read_status = read_message(sockfd, &msg, buff);

            // Si déconnexion du server
            if (read_status == 0) {
                 printf("\nThe server has closed the connection. Disconnecting...\n\n");
                 close(sockfd);
                 exit(0);
            } else if (read_status == -1) {
                 perror("\nConnection error detected");
                 printf("Server connection failed unexpectedly. Disconnecting...\n");
                 close(sockfd);
                 exit(1);
            }

			msg.infos[NICK_LEN-1] = '\0';
			msg.nick_sender[NICK_LEN-1] = '\0';
			buff[MSG_LEN-1] = '\0';
                
            if (msg.type == NICKNAME_ACCEPT) {
            
				printf("[SERVER] : Welcome on the chat %s\n", msg.infos);
				strncpy(my_nick, msg.infos, NICK_LEN-1);
				my_nick[NICK_LEN-1] = '\0';

			} 
			
			else if (msg.type == NICKNAME_DENIED) {
				printf("[SERVER] : Error: Nickname '%s' denied. Reason: %s\n", msg.infos, buff[0] != '\0' ? buff : "Nickname already taken or invalid.");
				printf("[SERVER] : Please choose another nickname with /nick <your_pseudo>\n");

			} 
			
			else if (msg.type == NICKNAME_LIST) {
                printf("[SERVER] : Online users are :\n%s\n", buff);

            } 

			else if (msg.type == NICKNAME_INFOS) {
                if (buff[0] != '\0') {
                    printf("[SERVER] : %s\n", buff);
                } else {
                    printf("[SERVER] : Error: User '%s' not found or not logged in.\n", msg.infos);
                }
            }

			else if (msg.type == BROADCAST_SEND){
				printf("[Message All from %s] : %s\n", msg.nick_sender, buff);
			}
			
			else if (msg.type == NICKNAME_INFOS) { 
                if (buff[0] != '\0') {
                    printf("[SERVER] : %s\n", buff);
                } else {
                    printf("[SERVER] : Error: User '%s' not found or not logged in.\n", msg.infos);
                }

            } 

			else if (msg.type == UNICAST_SEND) {
				printf("[Message from %s] : %s\n", msg.nick_sender, buff);
			}

			else if (msg.type == ECHO_SEND) {
				printf("[%s] : %s\n", msg.nick_sender, buff);
					
			}
        }
	}
}

int handle_connect(const char *server_name, const char *server_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(server_name, server_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "Connected to server ... done\n");
	fprintf(stderr, "[SERVER] : please login with /nick <your_pseudo>\n");
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {

	if (argc != 3){
		printf("Only put a server name and a server port\n");
		exit(EXIT_FAILURE);
	}

	int sfd;
	sfd = handle_connect(argv[1], argv[2]);
	echo_client(sfd);
	close(sfd);
	return EXIT_SUCCESS;
}
