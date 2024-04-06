#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>         /* gethostname */
#include <netinet/in.h>    /* struct sockaddr_in */

#define MAX_PLAYERS 100
#define MAX_NAME_LEN  51

void addnames(char names[][MAX_NAME_LEN], int *curr_num_players, const char *name) {

                if ((*curr_num_players) < MAX_PLAYERS) {
                        strcpy(names[*curr_num_players], name);
                        (*curr_num_players)++;
                        printf("Joined successfully!\n");
                }
                else{
                        printf("Maximum number of players have joined. Try again later :(");
                }


}

void error(char *msg) {
        perror(msg);
        exit(0);
}
int main(int argc, char **argv[])
{
        char playernames[MAX_PLAYERS][MAX_NAME_LEN];
        char name[MAX_NAME_LEN];
        int curr_num_players = 0;
        int sockfd, newsockfd, portno, clilen, n;
        char buffer[256]; // this buffer will store everything that will appear on the stdout
        struct sockaddr_in serv_addr, cli_addr;

        if (argc < 2)
                error("Error: no port provided");

        // creating a socket between the player and server

        // AF_INET -> Internet Domain
        // SOCK_STREAM -> type of socket
        // 0 - system chooses the most appropriate protocol - TCP or UDP depending on
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) // trouble creating a socket
                error("Error opening socket");

        // at this point, socket created,
        // free up buffer by using bzero to initialize it
        //
        bzero((char *) &serv_addr, sizeof(serv_addr));

        // get port number to connect to the player
        portno = atoi(argv[1]);

        serv_addr.sin_family = AF_INET; // should always be set to AF_INET

        // server address
        serv_addr.sin_port = htons(portno); // converts int to network byte order

        // The Ip address of localhost

        serv_addr.sin_addr.s_addr = INADDR_ANY;

        // Finally binding the socket to an address to send message

        if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
                error("Error in binding");

        // socket has been bound - listen for any new connections:
        listen(sockfd, 5); // second parameter: # of connections that can be waiting when process if handling a connection

        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0)
                error("Error in accepting the connection");


        printf("Name: ");
        fgets(name, 51, stdin);
        // name contains the name of the player

        addnames(playernames, &curr_num_players, name);
        // at this point, the names list is created and contains names of players

        bzero(buffer, 256); // initializing the buffer



}
