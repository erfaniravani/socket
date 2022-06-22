#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "tictoctoe.h"

int connectServer(int port) {
    int fd;
    struct sockaddr_in server_address;
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        printf("socket() failed\n");
    }

    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(port); 
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) { // checking for errors
        printf("Error in connecting to server\n");
    }

    return fd;
}

void alarm_handler(int sig) {
    return;
}

int connectClient(int port, int id , char** *final_board) {
    int sock, broadcast = 1, opt = 1;
    char buffer[1024] = {0};
    char nothing[1024] = {0};
    struct sockaddr_in bc_address;


    sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    bc_address.sin_family = AF_INET; 
    bc_address.sin_port = htons(port); 
    bc_address.sin_addr.s_addr = inet_addr("255.255.255.255");

    bind(sock, (struct sockaddr *)&bc_address, sizeof(bc_address));

    int no_send = 0;
    int no_rcv = 0;
    int turn = id;
    char row[2] = {0};
    char col[2] = {0};
    char winner;
    int draw;
    int rowind = row[0] - 'A';
    int colind = col[0] - '1';
    char** b = createboard();
    print(b);
    while (1) {
        signal(SIGALRM, alarm_handler);
        siginterrupt(SIGALRM, 1);
        if(turn == 1){ // get input and broadcast data
            printf("declare your move\n");
            memset(buffer, 0, 1024);
            memset(row, 0, 2);
            memset(col, 0, 2);
            while(1){
                alarm(20);
                read(0, buffer, 1024);
                if(strncmp(buffer, nothing, 1024) == 0){ // check if no input was recieved
                    no_send = 1;
                    break;
                }
                if(id == 1){
                    if(buffer[0] - '2' == 0){ // check the right format of data. player 1 should send data for player 2 starting with '2'
                        memcpy(row, &buffer[1], 1);
                        row[1] = '\0';
                        memcpy(col, &buffer[2], 1);
                        col[1] = '\0';
                        rowind = row[0] - 'A';
                        colind = col[0] - '1';
                        if(rowind < 3 && colind < 3){ // check the index 
                            if(b[rowind][colind] == ' '){ // check if the move is possible
                                b = play(b, row[0], col[0], id);
                                break;
                            }
                            else{
                                printf("Square occupied in client; try again.\n");
                            }
                        }
                        else
                            printf("wrong index; try again.\n");       
                    }
                    else{
                        printf("wrong format. try again\n");
                    }
                }
                if(id == 2){
                    if(buffer[0] - '1' == 0){ // check the right format of data. player 2 should send data for player 1 starting with '1'
                        memcpy(row, &buffer[1], 1);
                        row[1] = '\0';
                        memcpy(col, &buffer[2], 1);
                        col[1] = '\0';
                        //char** prev_board = b;
                        rowind = row[0] - 'A';
                        colind = col[0] - '1';
                        if(rowind < 3 && colind < 3){ // check the index
                            if(b[rowind][colind] == ' '){ // check if the move is possible
                                b = play(b, row[0], col[0], id);
                                break;
                            }
                            else
                                printf("Square occupied in client; try again.\n");
                        }
                        else{
                            printf("wrong index; try again.\n");
                        }
                    }
                    else{
                        printf("wrong format. try again\n");
                    }
                }
                
            }
            alarm(0);
            print(b);
            if(no_send == 1){ // time is up. no input recieved. a statement is sent to show there was no response
                printf("time out. changing turns...\n");
                if(id == 1)
                    sendto(sock, "2no move", strlen("2no move"), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
                if(id == 2)
                    sendto(sock, "1no move", strlen("1no move"), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
            }
            else if(no_send == 0){ 
                //response is broadcasted
                sendto(sock, buffer, strlen(buffer), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
                winner = winningmove(b, rowind, colind);
                draw = isdraw(b);
                if (winner == 'X'){ // if player 1 wins. data is sent for watchers and the function will return 1, the id of winner
                    char b_string[12] = {'3', '1', b[0][0],b[0][1], b[0][2], b[1][0], b[1][1], b[1][2], b[2][0], b[2][1], b[2][2]};
                    sendto(sock, b_string, strlen(b_string), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
                    printf("winner is player%d\n", 1);
                    printf("back to server...\n");
                    *final_board = b;
                    return 1;
                }
                else if(winner == 'O'){ // if player 2 wins. data is sent for watchers and the function will return 2, the id of winner
                    char b_string[12] = {'3', '2', b[0][0],b[0][1], b[0][2], b[1][0], b[1][1], b[1][2], b[2][0], b[2][1], b[2][2]};
                    sendto(sock, b_string, strlen(b_string), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
                    printf("winner is player%d\n", 2);
                    printf("back to server...\n");
                    *final_board = b;
                    return 2;
                }
                else if(draw == 1){ // the game ends in draw. data is sent for watchers. 0 is returned to show no one won
                    char b_string[12] = {'3', '0', b[0][0],b[0][1], b[0][2], b[1][0], b[1][1], b[1][2], b[2][0], b[2][1], b[2][2]};
                    sendto(sock, b_string, strlen(b_string), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
                    printf("game has no winner\n");
                    printf("back to server...\n");
                    *final_board = b;
                    return 0;
                }
                else{// send data to watchers
                    char b_string[12] = {'3', '3', b[0][0],b[0][1], b[0][2], b[1][0], b[1][1], b[1][2], b[2][0], b[2][1], b[2][2]};
                    sendto(sock, b_string, strlen(b_string), 0,(struct sockaddr *)&bc_address, sizeof(bc_address));
                }
            }
            no_send = 0;
        }
        if(turn == 2){ // recieve data
            memset(buffer, 0, 1024);
            memset(row, 0, 2);
            memset(col, 0, 2);
            printf("waiting for opponent\n");
            while(1){
                recv(sock, buffer, 1024, 0);
                if (buffer[0]-'0' == id){ //check the right format of data. player 1 should recieve data starting with '1'
                    if(strncmp(buffer, "2no move", 1024) == 0 || strncmp(buffer, "1no move", 1024) == 0){ // when the opponent wasnt responsive
                        printf("opponent didnt move. your turn again\n");
                        no_rcv = 1;
                    }
                    break;
                }
                else{
                    memset(buffer, 0, 1024);
                }
            }
            if(no_rcv == 0){ // if a proper move was recieved
                memcpy(row, &buffer[1], 1);
                row[1] = '\0';
                memcpy(col, &buffer[2], 1);
                col[1] = '\0';
                int move;
                if(buffer[0] == '2'){
                    move = 1;
                }
                else if(buffer[0] == '1'){
                    move = 2;
                }
                b = play(b, row[0], col[0], move);
                print(b);
                rowind = row[0] - 'A';
                colind = col[0] - '1';
                winner = winningmove(b, rowind, colind);
                draw = isdraw(b);
                if (winner == 'X'){ // if player 1 wins. the function will return 1, the id of winner
                    printf("winner is player%d\n", 1);
                    printf("back to server...\n");
                    *final_board = b;
                    return 1;
                }
                else if(winner == 'O'){ // if player 2 wins. the function will return 2, the id of winner
                    printf("winner is player%d\n", 2);
                    printf("back to server...\n");
                    *final_board = b;
                    return 2;
                }
                else if(draw == 1){ // the game ends in draw. 0 is returned to show no one won
                    printf("game has no winner\n");
                    printf("back to server...\n");
                    *final_board = b;
                    return 0;
                }
            }
            no_rcv = 0;
        }

        if(turn == 3){ // watchers
            while(1){
                memset(buffer, 0, 1024);
                recv(sock, buffer, 1024, 0);
                if (buffer[0]-'0' == id){ // check if the recieved data was ment to be for watchers
                    break;
                }
            }
            //print board
            printf(" |1|2|3|\n");
            for(int i = 0; i < 3; ++i) {
                printf("%c|", 'A' + i);
                for(int j = 0; j < 3; ++j) {
                    printf("%c|", buffer[3*i+j+2]);
                }
                printf("\n");
            }
            //check if the game is finished
            if(buffer[1]-'0' == 1 || buffer[1]-'0' == 2 || buffer[1]-'0' == 0){
                printf("game is finished.\n");
                return 1;
            }
        }
        //change turn between players
        if(turn == 1){
            turn = 2;
        }
        else if(turn ==2){
            turn = 1;
        }
        no_rcv = 0;
        no_send = 0;
    }

    return 0;
}



int main(int argc, char const *argv[]) {
    int fd;
    char buff[1024] = {0};
    char bas[6] = {0};
    fd = connectServer(8080);
    while (1) {
        read(0, buff, 1024);
        send(fd, buff, strlen(buff), 0);
        if(strncmp("play", buff, 4) == 0){ //when client wants to play. wait to get new port and play
            memset(buff, 0, 1024);
            printf("wait for party\n");
            recv(fd , bas, 6, 0);
            char new_port[5] = {0};
            char player_id[2] = {0};
            memcpy(new_port, bas, 4);
            memcpy(player_id, &bas[4], 1);
            player_id[1] = '\0';
            printf("new port : %s\n", new_port);
            printf("player id : %s\n", player_id);
            char** final_board;
            //game starts and clients connect to each other
            int outcome = connectClient(atoi(new_port), atoi(player_id), &final_board);
            //game ends and clients want to send game results to server. only player with id = 1 send data
            if(atoi(player_id) == 1){
                //winner array
                char w[2] = {0};
                //board array
                char b_string[11] = {final_board[0][0], final_board[0][1], final_board[0][2], final_board[1][0], final_board[1][1], final_board[1][2], final_board[2][0], final_board[2][1], final_board[2][2]};
                sprintf(w, "%d", outcome);
                strcat(b_string, w);
                strcat(b_string, new_port);
                //concatinate a CL to show that this message should not be eccoed in server
                strcat(b_string, "CL");
                send(fd, b_string, strlen(b_string), 0);
            }      
        }
        else if(strncmp("watch", buff, 5) == 0){ //when a client wants to watch
            //gets number of games that are still on going
            char port_cnt[3] = {0};
            recv(fd , port_cnt, 3, 0);
            for(int g = 0 ; g < atoi(port_cnt) ; g++){
                char data[1024] = {0};
                recv(fd , data, 1024, 0);
                printf("%s\n", data);
            }
            //adjust the number of sends and reveives to get all data
            if(atoi(port_cnt) > 0){
                printf("choose a port. EX. port8080\n");
                char prt[9] = {0};
                char gp[5] = {0};
                read(0, prt, 9);
                send(fd, prt, strlen(prt), 0);
                //check if the syntax in alright
                if(prt[0] == 'p' && prt[1] == 'o' && prt[2] == 'r' && prt[3] == 't'){
                    memcpy(gp, prt + 4, 4);
                    printf("you will be transfered to port %s\n", gp);
                    printf("X represents first and O represents second player\n");
                    //transfer to watch the game
                    char** bb;
                    int fin = connectClient(atoi(gp), 3, &bb);
                }     
                else
                    printf("wrong format");
            }
            else
                printf("no game at the moment\n");
            
        }
        else{
            memset(buff, 0, 1024);
        }
    }

    return 0;
}