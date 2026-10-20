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

#define MAX_CLIENTS 100

typedef struct client{
	int fd;
	struct sockaddr addr;
	socklen_t addr_len;
	char nickname[NICK_LEN];
	time_t conn_date;
	struct client *suivant;
} client;


struct client *creer_client(int fd, struct sockaddr addr, socklen_t addr_len){
	struct client *nv_client =malloc(sizeof(client));
	nv_client->fd = fd;
	nv_client->addr = addr;
	nv_client->addr_len = addr_len;
	nv_client->nickname[0] = '\0';
	nv_client->conn_date = time(NULL);
	nv_client->suivant = NULL;
	return nv_client;
}

void die(int status, char* str) {
    if (status == -1) {
        perror(str);
        exit(EXIT_FAILURE);
    }
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

void supprimer_client(client** liste_clients, int fd){
	client* client_actuel = *liste_clients;
	client* client_precedent = NULL;

	while (client_actuel != NULL && client_actuel->fd != fd){
		client_precedent = client_actuel;
		client_actuel = client_actuel->suivant;
	}

	if (client_actuel == NULL){
		return; 
	}

	if (client_precedent == NULL){ // cas où le client a supprimer est premier sur la liste
		*liste_clients = client_actuel->suivant;
	} else {
		client_precedent->suivant = client_actuel->suivant; 
	}

	close(client_actuel->fd);

	free(client_actuel);

}

int nickname_dispo(client* liste_clients, const char *nickname) {
    client* client_actuel = liste_clients;
    while (client_actuel != NULL) {
        if (client_actuel->nickname[0] != '\0' && strcmp(client_actuel->nickname, nickname) == 0) {
            return 1; // le pseudo est déjà pris
        }
        client_actuel = client_actuel->suivant;
    }
    return 0; // le pseudo est disponible
}


int handle_bind(const char *server_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, server_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		//utilisation de chatgpt 
		int yes = 1;
    	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    	    perror("setsockopt");
    	    close(sfd);
    	    continue;
    	}

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {

	client *liste_clients = NULL;

	if (argc != 2 ){
		printf("Only put a server port\n");
		exit(EXIT_FAILURE);
	}

	int sfd = handle_bind(argv[1]);
	die(listen(sfd, SOMAXCONN),"listen");

	printf("Server listening on port %s...\n", argv[1]);

	while (1) {
		struct pollfd pollfds[MAX_CLIENTS + 1]; // +1 pour le socket serveur
		int nfds = 0;
	
		// Ajouter la socket serveur
		pollfds[nfds].fd = sfd;
		pollfds[nfds].events = POLLIN;
		nfds++;
	
		// Ajouter les sockets des clients
		client* client_actuel = liste_clients;
		while (client_actuel != NULL && nfds < MAX_CLIENTS + 1) {
			pollfds[nfds].fd = client_actuel->fd;
			pollfds[nfds].events = POLLIN;
			nfds++;
			client_actuel = client_actuel->suivant;
		}
	
		int ret = poll(pollfds, nfds, -1);
		die(ret, "poll");
	
		for (int i = 0; i < nfds; i++) {
	
			// Nouvelle connexion
			if (pollfds[i].fd == sfd && (pollfds[i].revents & POLLIN)) {
				struct sockaddr cli_addr;
				socklen_t len = sizeof(cli_addr);
				int new_fd = accept(sfd, &cli_addr, &len);
				die(new_fd, "accept");
	
				printf("New client connected: fd= %d\n", new_fd);
	
				client* nouveau = creer_client(new_fd, cli_addr, len);
				nouveau->suivant = liste_clients;
				liste_clients = nouveau;

				printf("Liste actuelle des clients connectés :\n");
				client* tmp = liste_clients;
				while (tmp != NULL) {
				    struct sockaddr_in *addr_in = (struct sockaddr_in *)&(tmp->addr);
				    char ip_str[INET_ADDRSTRLEN];
				    inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);
				    printf(" - fd = %d | IP = %s | Port = %d\n", tmp->fd, ip_str, ntohs(addr_in->sin_port));
				    tmp = tmp->suivant;
				}

			}
	
			// Client existant envoie un message
			else if (pollfds[i].revents & POLLIN) {
				int fd_client = pollfds[i].fd;
				struct message msg_recu;
				int nb_received = 0;
	
				int ret_read;
				while (nb_received != sizeof(msg_recu)) {
					ret_read = read(fd_client, ((char *)&msg_recu) + nb_received, sizeof(msg_recu) - nb_received);
					if (ret_read <= 0) {
						printf("Client disconnected while reading size: fd= %d\n", fd_client);
						supprimer_client(&liste_clients, fd_client);
						break;
					}
					nb_received += ret_read;
				}
				if (ret_read <= 0) {
					continue;
				}

				char buff[MSG_LEN] = {0};
				int nb_to_read = msg_recu.pld_len;
				nb_received = 0;
	
				while (nb_received != nb_to_read) {
					ret_read = read(fd_client, buff + nb_received, nb_to_read - nb_received);
					if (ret_read <= 0) {
						printf("Client disconnected while reading message: fd= %d\n", fd_client);
						supprimer_client(&liste_clients, fd_client);
						break;
					}
					nb_received += ret_read;
				}

				if (ret_read <= 0){
					continue;
				} 

				buff[nb_received] = '\0';

				client* current_cl = liste_clients;
                while (current_cl != NULL && current_cl->fd != fd_client) {
                    current_cl = current_cl->suivant;
                }
                
				// Le client n'a pas encore défini de pseudo et essaie d'envoyer autre chose que /nick
                if (msg_recu.type != NICKNAME_NEW && current_cl != NULL && current_cl->nickname[0] == '\0') {
                    
                    struct message error_msg;
                    memset(&error_msg, 0, sizeof(error_msg));
                    error_msg.type = NICKNAME_DENIED;
                    strncpy(error_msg.nick_sender, "Server", NICK_LEN-1);
                    const char *error_text = "You must use /nick <your_pseudo> to identify before sending other messages.";
                    error_msg.pld_len = strlen(error_text);

                    error_msg.infos[0] = '\0';
                    write_message(fd_client, &error_msg, error_text);

                    printf("Denied non-nick command from unauthenticated fd=%d.\n", fd_client);
                    continue;
                }

				if (msg_recu.type == NICKNAME_NEW) { // Relatif à /nick
                    
                    char *new_nick = msg_recu.infos; 
					int is_valid = 1;

					char reason_buff[MSG_LEN] = {0};

                    // Vérification du pseudo
                    int check = 1;
					for (int i = 0; i < strlen(new_nick); i++){
						if (!isalnum(new_nick[i])) {
							check = 0;
							break;
						}
					}

					// Vérification de longueur 
					if (strlen(new_nick) == 0 || strlen(new_nick) >= NICK_LEN || check == 0) {
						is_valid = 0;

                        if (check== 0) {
                             strncpy(reason_buff, "Nickname contains invalid characters (alphanumeric only).", MSG_LEN - 1);
                        } else {
                             strncpy(reason_buff, "Nickname length is invalid (must be 1 to 127 characters).", MSG_LEN - 1);
                        }
					}

                    // Récupérer le client
                    client* cl = liste_clients;
                    while (cl != NULL && cl->fd != fd_client) {
                        cl = cl->suivant;
                    }
                    
                    // Vérifie si le pseudo est disponible
                    if (is_valid && nickname_dispo(liste_clients, new_nick)) {
                        is_valid = 0;
                        strncpy(reason_buff, "Nickname is already taken.", MSG_LEN - 1);
                    }

                    // Création de la réponse
                    struct message nick_msg;
                    memset(&nick_msg, 0, sizeof(nick_msg));
					strncpy(nick_msg.nick_sender, "Server", NICK_LEN-1);
                    strncpy(nick_msg.infos, new_nick, NICK_LEN-1);
                    nick_msg.pld_len = 0;

                    if (is_valid) {
                        nick_msg.type = NICKNAME_ACCEPT;  
                        if (cl != NULL) {
                            strncpy(cl->nickname, new_nick, NICK_LEN-1);
                            cl->nickname[NICK_LEN-1] = '\0';
                        }
                        printf("Client fd=%d updated nickname to '%s'.\n", fd_client, new_nick);
                        write_message(fd_client, &nick_msg, "");

                    } else {
                        nick_msg.type = NICKNAME_DENIED;
                        printf("Client fd=%d nickname request for '%s' denied. Reason: %s\n", fd_client, new_nick, reason_buff);
                        nick_msg.pld_len = strlen(reason_buff);
                        write_message(fd_client, &nick_msg, reason_buff);
                    }

                    continue;

                } else if (msg_recu.type == NICKNAME_LIST) { // Relatif à /who
                    
                    char users_list[MSG_LEN] = {0};
                    char temp_line[NICK_LEN + 5];
                    
                    client* current_client = liste_clients;

                    while (current_client != NULL) {

                        const char *display_nick;
                        if (current_client->nickname[0] != '\0') {
                            display_nick = current_client->nickname;
                        } else {
                            display_nick = "Unknown"; // cas où le client n'a pas encore de pseudo
                        }
                        
                        // Création de la ligne de l'utilisateur
                        temp_line[0] = '\0'; 
						strcat(temp_line, "\t\t\t\t - ");
						strncat(temp_line, display_nick, NICK_LEN);
						strcat(temp_line, "\n");
                        
                        // Vérifier si ajouter la ligne dépasse MSG_LEN
                        if (strlen(users_list) + strlen(temp_line) < MSG_LEN) {
                            strcat(users_list, temp_line);
                        } else {
                            break; 
                        }
                        
                        current_client = current_client->suivant;
                    }
                    
                    // Création du message de réponse
                    struct message list_msg;
                    memset(&list_msg, 0, sizeof(list_msg));
                    list_msg.type = NICKNAME_LIST;
                    strncpy(list_msg.nick_sender, "Serveur", NICK_LEN-1);
                    list_msg.pld_len = strlen(users_list);
                    
                    printf("Sending user list to fd=%d.\n", fd_client);
                    write_message(fd_client, &list_msg, users_list);

                    continue; // Traitement terminé pour ce client
                }

				else if(msg_recu.type == BROADCAST_SEND){	 // Relatif à /msgall
					client* sender = liste_clients;
    				while (sender != NULL && sender->fd != fd_client) {
    				    sender = sender->suivant;
    				}
				
    				char *sender_nick;
					sender_nick = sender->nickname;

    				printf("Broadcast from %s: %s\n", sender_nick, buff);
				
    				// Construire le message à envoyer
    				struct message broadcast_msg;
    				memset(&broadcast_msg, 0, sizeof(broadcast_msg));
    				broadcast_msg.type = BROADCAST_SEND;
    				strncpy(broadcast_msg.nick_sender, sender_nick, NICK_LEN-1);
    				broadcast_msg.pld_len = strlen(buff);
				
    				// Parcourir tous les clients
    				client* dest = liste_clients;
    				while (dest != NULL) {
    				    if (dest->fd != fd_client) { // ne pas renvoyer à l’expéditeur
    				        write_message(dest->fd, &broadcast_msg, buff);
    				    }
    				    dest = dest->suivant;
    				}
				
    				continue;
				}

				else if (msg_recu.type == NICKNAME_INFOS) { // Relatif à /whois

                    char *target_nick = msg_recu.infos;
                    char info_response[MSG_LEN] = {0}; 

                    client* target_cl = liste_clients;

                    while (target_cl != NULL) {
                        if (target_cl->nickname[0] != '\0' && strcmp(target_cl->nickname, target_nick) == 0) {
                            break;
                        }
                        target_cl = target_cl->suivant;
                    }

                    if (target_cl != NULL) {
                        struct sockaddr_in *addr_in = (struct sockaddr_in *)&(target_cl->addr);
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);
                        
                        // Utilisation de l'IA pour réaliser le formatage la date (2014/09/29@19:23)
                        char date_str[64];
                        struct tm *tm_info = localtime(&(target_cl->conn_date));
                        strftime(date_str, sizeof(date_str), "%Y/%m/%d@%H:%M", tm_info);
                        
                        snprintf(info_response, MSG_LEN,
                                 "%s is connected since %s with IP address %s and port number %d",
                                 target_cl->nickname, date_str, ip_str, ntohs(addr_in->sin_port));

                    } else {
                        printf("Whois request failed: User '%s' not found.\n", target_nick);
                        snprintf(info_response, MSG_LEN, "Error: User '%s' not found or not logged in.", target_nick);
                    }

                    struct message info_msg;
                    memset(&info_msg, 0, sizeof(info_msg));
                    info_msg.type = NICKNAME_INFOS;
                    strncpy(info_msg.nick_sender, "Serveur", NICK_LEN-1);
                    strncpy(info_msg.infos, target_nick, INFOS_LEN-1);
                    
                    info_msg.pld_len = strlen(info_response);
                    write_message(fd_client, &info_msg, info_response);

                    continue;
                }

				else if(msg_recu.type == UNICAST_SEND){ // Relatif à /msg
					client* dest_client = liste_clients;
					while (dest_client != NULL) {
					    if (dest_client->nickname[0] != '\0' && strcmp(dest_client->nickname, msg_recu.dest) == 0) {
					        break;
					    }
					    dest_client = dest_client->suivant;
					}
				
					if (dest_client != NULL) {
					    client* sender = liste_clients;
					    while (sender != NULL && sender->fd != fd_client) {
					        sender = sender->suivant;
					    }
					
					    char *sender_nick;
						sender_nick = sender->nickname;
					
					    printf("Unicast from %s to %s: %s\n", sender_nick, dest_client->nickname, buff);
					
					    // Construire le message à envoyer
					    struct message unicast_msg;
					    memset(&unicast_msg, 0, sizeof(unicast_msg));
					    unicast_msg.type = UNICAST_SEND;
					    strncpy(unicast_msg.nick_sender, sender_nick, NICK_LEN-1);
					    unicast_msg.pld_len = strlen(buff);
					
					    write_message(dest_client->fd, &unicast_msg, buff);
				
					} else {
					    // Le destinataire n'existe pas
					    struct message error_msg;
					    memset(&error_msg, 0, sizeof(error_msg));
					    error_msg.type = NICKNAME_INFOS;
					    strncpy(error_msg.nick_sender, "Serveur", NICK_LEN-1);
					    strncpy(error_msg.infos, msg_recu.dest, INFOS_LEN-1);
					    const char *error_text = "User not found or not logged in.";
					    error_msg.pld_len = strlen(error_text);
					    write_message(fd_client, &error_msg, error_text);
					
					    printf("Unicast failed: User '%s' not found for fd=%d.\n", msg_recu.dest, fd_client);
					}
				
					continue;
				}

				client* cl = liste_clients;
				while (cl != NULL) {
					if (cl->fd == fd_client) break;
					cl = cl->suivant;
				}

				printf("Received from fd= %d | sender=%s | type=%d | infos=%s | msg=%s\n", fd_client, msg_recu.nick_sender, msg_recu.type, msg_recu.infos, buff);

				// Gestion de la déconnexion avec /quit
				if (strcmp(buff, "/quit") == 0) {
					printf("Client on fd = %d disconnected with /quit\n", fd_client);
					supprimer_client(&liste_clients, fd_client);
					continue;
				}
	
				// Créer le message à renvoyer
				struct message msg_to_send;
				memset(&msg_to_send, 0, sizeof(msg_to_send));
				msg_to_send.type = ECHO_SEND;

				strncpy(msg_to_send.nick_sender, cl->nickname, NICK_LEN-1);

				msg_to_send.pld_len = strlen(buff);
				write_message(fd_client, &msg_to_send, buff);

			}
		}
	}
	
    close(sfd);
    return EXIT_SUCCESS;
}
