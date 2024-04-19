/*
 * socket demonstrations:
 * This is the server side of an "internet domain" socket connection, for
 * communicating over the network.
 *
 * In this case we are willing to wait for chatter from the client
 * _or_ for a new connection.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <time.h>
#include <arpa/inet.h>

#ifndef PORT
    #define PORT 5962
#endif

# define SECONDS 10

struct client {
    int fd;
    struct in_addr ipaddr;
    struct client *next;
    char name[100];   // keep the client's name
    int in_game;      // 0 if waiting for a game, 1 if in a game
    struct client *last_opponent; // pointer to their last opponent
    int hitpoints;
    int powermoves;
    int turn; // 1 if their turn, 0 otherwise
    int mute; // 1 if the client is muted, 0 otherwise
};


static struct client *addclient(struct client *top, int fd, struct in_addr addr, char *name);

static struct client *removeclient(struct client *top, int fd);

static void broadcast(struct client *top, char *s, int size);

int handleclient(struct client *p, struct client *top);
static void broadcast_except(struct client *client_list, char *msg, int except_fd);
void powermove(struct client *p1, struct client *p2);
void speak(struct client *p1, struct client *p2);
void mute(struct client *p1);
void attack(struct client *p1, struct client *p2);
void handle_new_connection(int listenfd, struct client **head, fd_set *all_fds);
void attempt_matchmaking(struct client **head);
char get_move(struct client *current_player);
void start_game(struct client *p1, struct client *p2);
void send_status(struct client *player);

void end_game(struct client *winner, struct client *loser);
int bindandlisten(void);

void attempt_matchmaking(struct client **head) {
    struct client *p, *q;

    for (p = *head; p != NULL; p = p->next) {
        for (q = *head; q != NULL; q = q->next) {
            if (!p->in_game && p != q && !q->in_game && p->last_opponent != q && q->last_opponent != p) {
                // init the match
                p->in_game = 1;
                q->in_game = 1;
                p->last_opponent = q;
                q->last_opponent = p;

                // notify players of the match
                char match_msg[512];
                sprintf(match_msg, "match found! You are now playing against %s\n", q->name);
                send(p->fd, match_msg, strlen(match_msg), 0);
                sprintf(match_msg, "match found! You are now playing against %s\n", p->name);
                send(q->fd, match_msg, strlen(match_msg), 0);

                // start game between the two clients
                start_game(p, q);
                return; // end the matchmaking, match has been made
            }
        }
    }
}

// broadcast to everyone but new client
void broadcast_except(struct client *client_list, char *msg, int except_fd) {
    struct client *tmp = client_list;
    while (tmp != NULL) {
        if (tmp->fd != except_fd) {  // exclude the new client
            send(tmp->fd, msg, strlen(msg), 0);
        }
        tmp = tmp->next;
    }
}

void start_game(struct client *p1, struct client *p2) {
    // init game state
    p1->mute = 0;
    p2->mute = 0;
    p1->hitpoints = 20;
    p2->hitpoints = 20;
    p1->powermoves = 3;  // make random
    p2->powermoves = 3;  // make random

    p1->turn = rand() % 2;

    if (p1->turn == 1){
    p2->turn = 0;}
    else{
    p2->turn= 1;
    }

    // notify players that a game started
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "\nYou engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n",
             p2->name, p1->hitpoints, p1->powermoves);
    send(p1->fd, buffer, strlen(buffer), 0);

    snprintf(buffer, sizeof(buffer), "\nYou engage %s!\nYour hitpoints: %d\nYour powermoves: %d\n",
             p1->name, p2->hitpoints, p2->powermoves);
    send(p2->fd, buffer, strlen(buffer), 0);

    // rest of the game logic
        while (p1->hitpoints > 0 && p2->hitpoints > 0) {
            struct client *current_player = p1->turn ? p1 : p2;
            struct client *opponent = p1->turn ? p2 : p1;

            send_status(p1);
            send_status(p2);

            // Prompt current player for their move
            char move = get_move(current_player);

            // If move is valid, execute it
            if (move == 'a') {
            attack(current_player, opponent);

            } else if (move == 'p') {
            powermove(current_player, opponent);


            } else if (move == 's') {
                // send a message to the opponent
                speak(current_player, opponent);
            
            } else if (move == 'm'){
                // mute the opponent
                mute(current_player);

            } else if (move == 'i') {
            char move = get_move(current_player); // not tested yet
            }
            // check if the game ended or not
            if (opponent->hitpoints <= 0) {
                end_game(current_player, opponent);
            }



        }
}

void mute(struct client *p1){
    char muted_msg[256];
    sprintf(muted_msg, "\nYou have muted your opponent, their messages will no longer reach you.\n");
    write(p1->fd, muted_msg, strlen(muted_msg));
    p1->mute = 1;
}

void speak(struct client *p1, struct client *p2){
    char chat_msg[512];
    char prompt_buffer[256];
    char incoming_msg[256];
    sprintf(prompt_buffer, "\nSay Something:\n");
    write(p1->fd, prompt_buffer, strlen(prompt_buffer));

    //char client_name[100];
    memset(chat_msg, 0, sizeof(chat_msg));  // Clear the buffer
    int name_len = 0;
    char ch;
    while (read(p1->fd, &ch, 1) > 0 && ch != '\n' && name_len < sizeof(chat_msg) - 1) {
        chat_msg[name_len++] = ch;
    }
    chat_msg[name_len] = '\0';  // Null-terminate the string
    
    // If the opponent has not muted send message
    if(!p2->mute){
        sprintf(incoming_msg, "Your opponent says:\n");
        write(p2->fd, incoming_msg, strlen(incoming_msg));
        write(p2->fd, chat_msg, strlen(chat_msg));
    }
    return;
}

void end_game(struct client *winner, struct client *loser) {
    char end_msg_winner[256];
    char end_msg_loser[256];

    // Construct the end game message for the winner
    snprintf(end_msg_winner, sizeof(end_msg_winner),
             "\nThe match has ended. You have won!\n");

    // Construct the end game message for the loser
    snprintf(end_msg_loser, sizeof(end_msg_loser),
             "\nThe match has ended. You have lost!\n");

    // Send the end game message to the winner
    if (send(winner->fd, end_msg_winner, strlen(end_msg_winner), 0) == -1) {
        perror("send end game message to winner error");
    }

    // send the end game message to the loser
    if (send(loser->fd, end_msg_loser, strlen(end_msg_loser), 0) == -1) {
        perror("send end game message to loser error");
    }

    // mark both clients as not in game anymore
    winner->in_game = 0;
    loser->in_game = 0;
}
void send_status(struct client *player) {
    char status[256];

    // Construct the status message
    snprintf(status, sizeof(status),
             "\nIt's %s your turn!\nYour hitpoints: %d\nYour powermoves: %d\n",
             (player->turn ? "" : "not"), // This adds "not" if it's not the player's turn.
             player->hitpoints,
             player->powermoves);

    // Send the status message to the player
    if (send(player->fd, status, strlen(status), 0) == -1) {
        perror("send status error");

    }
}

char get_move(struct client *current_player) {
    char move;
    char buffer[256];
    int len;

    // ask client for move
    char *prompt = "\n(a)ttack\n(p)owermove\n(s)peak something\n(m)ute\n";
    send(current_player->fd, prompt, strlen(prompt), 0);

    // raed the move from the client
    len = read(current_player->fd, &move, sizeof(move));
    if (len > 0) {

        if (move == 'a' || move == 'p' || move == 's' || move == 'm') {
            return move; // valid move
        } else { //invalid move
            return 'i';
        }
    } else if (len == 0) {
        // the client disconnected
        return 'd';
    } else {
        // An error occurred
        perror("read from client");
        return 'e'; // Error
    }
}
int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    maxfd = listenfd;

    while (1) {
    // make a copy of the set before we pass it into select
        rset = allset;
        tv.tv_sec = SECONDS;
        tv.tv_usec = 0;

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %d seconds\n", SECONDS);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        // adding a new client connection
        if (FD_ISSET(listenfd, &rset)) {
                handle_new_connection(listenfd, &head, &allset);
                attempt_matchmaking(&head);  // try to match clients after a new connection
            }

        // data from other clients
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i,  &rset) && i != listenfd) {
                p = head;
                while (p != NULL && p->fd != i) {
                    p = p->next;
                }
                if (p == NULL) {
                    continue; // no client found with fd i so continue
                }
                int result = handleclient(p, head);
                if (result == -1) {
                    int tmp_fd = p->fd;
                    head = removeclient(head, p->fd);
                    FD_CLR(tmp_fd, &allset);
                    close(tmp_fd);
                    printf("%s has left the game.\n", p->name);
                    attempt_matchmaking(&head);  // try to match clients after someone leaves
                }
            }
        }
    }
    return 0;
}

int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        printf("Received %d bytes: %s", len, buf);
        sprintf(outbuf, "%s says: %s", inet_ntoa(p->ipaddr), buf);
        broadcast(top, outbuf, strlen(outbuf));
        return 0;
    } else if (len <= 0) {
        // socket is closed
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
        broadcast(top, outbuf, strlen(outbuf));

        if (p->in_game) { // check if the client was in a game
            p->in_game = 0; // mark client as no longer in a game
            char message[512];
            sprintf(message, "%s has finished their game and is now available.", p->name);
            broadcast_except(top, message, p->fd);
        }

        return -1;
    }
    return -1;
}

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr, char *name) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    // init the client structure
    p->fd = fd;
    p->ipaddr = addr;
    p->in_game = 0;
    strncpy(p->name, name, sizeof(p->name));  // copy the name provided as  parameter
    p->name[sizeof(p->name) - 1] = '\0';

    // add to the front of the list
    p->next = top;
    top = p;

    printf("Added client %s\n", p->name);
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}

void powermove(struct client *p1, struct client *p2) {
        char buffer[256];
        int target = rand() % 2; // generates a random int btn 0 and 1
        // if target = 0 -> did not hit the target
        // target = 1 -> hit the target

        srand(time(NULL));

        if ((p1 -> powermoves) > 0) {
                if (target) { // if miss = 1

                    int p_deducted = ((rand() % 5) + 2)*3; // this chooses a random number between 2 and 6
                                                                // multiplied by 3 to cause three times the attack

                    p1->powermoves -= 1;
                    p2->hitpoints -= p_deducted;

                    sprintf(buffer, "\nYou hit your opponent's target for %d points\n", p_deducted);
                    write(p1->fd, buffer, strlen(buffer)); // writing to the p1's screen


                    // possible bug check here?

                    sprintf(buffer, "\nYour opponent hit you with a powermove. You lost %d points! \n", p_deducted);
                    write(p2->fd, buffer, strlen(buffer));


                }
                else { // the powermove didn't hit the target
                       p1->powermoves -= 1;
                       sprintf(buffer, "\nYour powermove missed the opponent!\n");
                       write(p1->fd, buffer, strlen(buffer));

                       sprintf(buffer, "\nYour opponent missed you with their powermove!\n");
                       write(p2->fd, buffer, strlen(buffer));
                }
                p1->turn =!p1->turn;
                p2->turn = !p2->turn;

                }
}

// p1 attacks p2
void attack(struct client *p1, struct client *p2) {
        char buffer[256];
        srand(time(NULL));
        int power = (rand() % 5) + 2; // chooses a random number between 2 to 6
        p2->hitpoints -= power;
        sprintf(buffer, "\nYou hit your opponent with a damage of %d points \n", power);
        write(p1->fd, buffer, strlen(buffer));

        sprintf(buffer, "\nYour opponent hit you with damage of %d points \n", power);
        write(p2->fd, buffer, strlen(buffer));

        p1->turn =!p1->turn;
        p2->turn = !p2->turn;
}

void handle_new_connection(int listenfd, struct client **head, fd_set *all_fds) {
    struct sockaddr_in cli_addr;
    socklen_t addrlen = sizeof(cli_addr);
    int clientfd = accept(listenfd, (struct sockaddr *)&cli_addr, &addrlen);
    if (clientfd < 0) {
        perror("Server: accept failed");
        return;
    }

    char welcome_msg[] = "Enter your name: ";
    send(clientfd, welcome_msg, sizeof(welcome_msg) - 1, 0);  // Exclude null terminator from send

    char client_name[100];
    memset(client_name, 0, sizeof(client_name));  // Clear the buffer
    int name_len = 0;
    char ch;
    while (read(clientfd, &ch, 1) > 0 && ch != '\n' && name_len < sizeof(client_name) - 1) {
        client_name[name_len++] = ch;
    }
    client_name[name_len] = '\0';  // Null-terminate the string

    if (name_len <= 0) {
        printf("Failed to read name from client.\n");
        close(clientfd);
        return;
    }

    // Now adding the client using addclient
    *head = addclient(*head, clientfd, cli_addr.sin_addr, client_name);
    FD_SET(clientfd, all_fds);

    // Message to user who joins
    char *wait_msg = "Welcome! You are now awaiting an opponent.\n";
    send(clientfd, wait_msg, strlen(wait_msg), 0);

    // Message to everyone else notifying someone new joined
    char broadcast_msg[200];
    sprintf(broadcast_msg, "%s has entered the arena.\n", client_name);
    broadcast_except(*head, broadcast_msg, clientfd);
}


static void broadcast(struct client *top, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        write(p->fd, s, size);
    }
    /* should probably check write() return value and perhaps remove client */
}