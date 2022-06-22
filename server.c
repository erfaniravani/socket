#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>


struct GameInfo{
    int p1_fd;
    int p2_fd;
    int port;
    int status;
    int winner;
    char board[10];
};

int setupServer(int port) {
    struct sockaddr_in address;
    int server_fd;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        printf("socket() failed\n");
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    int b = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if(b < 0){
        printf("bind() failed\n");
    }

    int l = listen(server_fd, 4);
    if(l < 0){
        printf("listen() failed\n");
    }

    return server_fd;
}

int acceptClient(int server_fd) {
    int client_fd;
    struct sockaddr_in client_address;
    int address_len = sizeof(client_address);
    client_fd = accept(server_fd, (struct sockaddr *)&client_address, (socklen_t*) &address_len);
    if(client_fd < 0){
        printf("accept() failed\n");
    }

    return client_fd;
}




int main(int argc, char const *argv[]) {
    int server_fd, new_socket, max_sd;
    //create a struct for ongoing game informations
    struct GameInfo gi[10];
    //initialze the struct. status zero means there is no game at that moment 
    for(int i = 0 ; i < 10 ; i++){
        gi[i].status = 0;
        gi[i].port = 0;
    }
    ///////////////////
    char buffer[1024] = {0};
    char play[2] = {0};
    int players = 0;
    char m1[6] = {0};
    char m2[6] = {0};
    fd_set master_set, working_set;
    int server_cnt = 1;
    server_fd = setupServer(8080);

    FD_ZERO(&master_set);
    max_sd = server_fd;
    FD_SET(server_fd, &master_set);

    write(1, "Server is running\n", 18);

    int connected = 0;
    while (1) {
        working_set = master_set;
        select(max_sd + 1, &working_set, NULL, NULL, NULL);

        for (int i = 0; i <= max_sd; i++) {
            if (FD_ISSET(i, &working_set)) {
                
                if (i == server_fd) {  // new clinet
                    new_socket = acceptClient(server_fd);
                    FD_SET(new_socket, &master_set);
                    if (new_socket > max_sd)
                        max_sd = new_socket;
                    printf("New client connected. fd = %d\n", new_socket);
                }
                
                else { // client sending msg
                    int bytes_received;
                    bytes_received = recv(i , buffer, 1024, 0);
                    
                    if (bytes_received == 0) { // EOF
                        printf("client fd = %d closed\n", i);
                        close(i);
                        FD_CLR(i, &master_set);
                        continue;
                    }

                    if(buffer[14] == 'C' && buffer[15] == 'L'){ // getting the data when a game is over. if it has a CL on 
                        int idx;                                // specific locations means it should not be eccoed on server and should
                        char game_port[5] = {0};                // be saved in file
                        memcpy(game_port, buffer + 10, 4);
                        //find the index of game in our struct using its port
                        for(int v = 0; v < 10; v++){
                            if(gi[v].port == atoi(game_port))
                                idx = v;
                        }
                        //update the informations in the stuct
                        char winner_player[2] = {0};
                        char game_board[10] = {0};
                        memcpy(game_board, buffer, 9 * sizeof(char));
                        memcpy(winner_player, buffer + 9, 1 * sizeof(char));
                        if(atoi(winner_player) == 1){
                            gi[idx].winner = gi[idx].p1_fd;
                        }
                        else if(atoi(winner_player) == 2){
                            gi[idx].winner = gi[idx].p2_fd;
                        }
                        else{
                            gi[idx].winner = 0;
                        }
                        memcpy(gi[idx].board, game_board, 9);
                        gi[idx].status = 0;
                        //write data into a file
                        char file_data[84];
                        sprintf(file_data, "player%d and player%d on port %d. winner is %d\n", gi[idx].p1_fd, gi[idx].p2_fd, gi[idx].port, gi[idx].winner);
                        int output_fd = open("out.txt", O_WRONLY | O_CREAT | O_APPEND);
                        write(output_fd, file_data, strlen(file_data));
                        write(output_fd, " |1|2|3|\n", strlen(" |1|2|3|\n"));
                        for(int i = 0; i < 3; ++i) {
                            char file_data_2[5];
                            sprintf(file_data_2, "%c|", 'A' + i);
                            write(output_fd, file_data_2, strlen(file_data_2));
                            for(int j = 0; j < 3; ++j) {
                                char file_data_3[5];
                                sprintf(file_data_3, "%c|", gi[idx].board[3*i+j]);
                                write(output_fd, file_data_3, strlen(file_data_3));
                            }
                            write(output_fd, "\n", strlen("\n"));
                        }
                        close(output_fd);
                    }
                    else{ // ecco whatever clients say
                        printf("client %d: %s\n", i, buffer);
                    }
                    
                    if(strncmp("play", buffer, 4) == 0){ // when a client wants to play
                        //check if the same client has requested the game again
                        if(play[0] != i && play[1] != i){
                            play[players] = i;
                            players++;
                            if(players >= 2){
                                players = 0;
                            }
                            printf("your in list to play now: p1 = %d p2 = %d\n", play[0], play[1]);
                        }
                        //create the game 
                        if(play[0] != 0 && play[1] != 0){
                            printf("you are going to the game: p1 = %d p2 = %d\n", play[0], play[1]);
                            //check if there is a free space 
                            int free_index;
                            int is_free;
                            for(int l = 0; l < 10; l++){
                                if(gi[l].status == 0){
                                    is_free = 1;
                                    free_index = l;
                                }
                            }
                            if(is_free == 1){
                                int port_num = 8080+server_cnt;
                                gi[free_index].p1_fd = play[0];
                                gi[free_index].p2_fd = play[1];
                                gi[free_index].status = 1;
                                gi[free_index].port = port_num;
                                ////////////////////////////////
                                sprintf(m1, "%d", port_num);
                                sprintf(m2, "%d", port_num);
                                strcat(m1, "1");
                                strcat(m2, "2");
                                send(play[0], m1, strlen(m1), 0);
                                send(play[1], m2, strlen(m2), 0);
                                memset(play, 0, 2);
                                server_cnt = server_cnt + 1;
                            }
                        }
                    }
                    else if(strncmp("watch", buffer, 5) == 0){ // when a client wants to watch a game
                        //find how many games are in progress
                        int available_ports = 0;
                        for(int h = 0 ; h < 10 ; h++){
                            if(gi[h].status == 1){
                                available_ports = available_ports + 1;
                            }
                        }
                        //send the number of available ports to client in order to adjuct the number of sends and recieves
                        char pm[3];
                        sprintf(pm, "%d", available_ports);
                        send(i, pm, strlen(pm), 0);
                        //send the ports with their information
                        for(int h = 0 ; h < 10 ; h++){
                            if(gi[h].status == 1){
                                char data[1024] = {0};
                                sprintf(data,"mplayers %d & %d, playing on port %d\n", gi[h].p1_fd, gi[h].p2_fd, gi[h].port);
                                send(i, data, 1024, 0);
                            }
                        }
                    }
                    memset(buffer, 0, 1024);
                }
            }
        }

    }

    return 0;
}