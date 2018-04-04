/**
 * @client
 * @author  hao wang <hwang67@buffalo.edu> yue wan <ywan3@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * Contains logging functions to be used by CSE489/589 students.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "../include/global.h"
#include "../include/logger.h"
#include "../include/client.h"
#include "../include/server.h"

//socket file discriptor that connect to server
int sockfd;
char localIP[20];


char clientList[512];     //client list that server send back
struct clientInfo clients[4];  //current log-in clients
struct clientInfo historyClients[4];  //history log-in clients
int logedIncliCnt;        //current logged in client count
int historyCliCnt;
//select() params
fd_set clientMasterfd;    //master file discriptor list
fd_set clientRead_fds;  //temp file discriptor list for select()
int clientfdmax;        //maximum file descriptor number

char *blockList[4]; //block list
int blockcnt;
//if current host has logged in to the server
int logined = 0;

int getOwnIP(){
    // if (localIP[0] != '\0'){
    //     return 0;
    // }

    struct addrinfo hints, *res;
    int getIPsockfd;
    socklen_t addr_len;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    getaddrinfo("8.8.8.8", "53", &hints, &res);

    //make a socket;
    if ((getIPsockfd = socket(res -> ai_family, res -> ai_socktype, res -> ai_protocol)) == -1){
        perror("fail to create socket");
    }
    //connect
    if (connect(getIPsockfd, res -> ai_addr, res -> ai_addrlen) == -1){
        close(getIPsockfd);
        perror("fail to connect");
    }
    struct sockaddr_in localAddr;
    addr_len = sizeof localAddr;

    if (getsockname(getIPsockfd, (struct sockaddr *)&localAddr, &addr_len) == -1){
        perror("fail to get sock name");
    }
    //turn address struct to dot notation
    const char *p = inet_ntop(AF_INET, &localAddr.sin_addr, localIP, sizeof(localIP));
    close(getIPsockfd);

    return 0;
}

//check ip vaildiation
int checkIPVaild(char *ip){
    //check if ip is vaild
    struct sockaddr_in sa;
    int res = inet_pton(AF_INET, ip, &(sa.sin_addr));
    if (res < 1){
        return -1;
    }
     //check if it is the host
     if (localIP[0] == '\0' ){
        getOwnIP();
     }
     if (!strcmp(ip, localIP)){
        return -1;
     }

     //check if the ip exist in the local copy of list
     for(int i = 0; i < historyCliCnt; i++){
         if(!strcmp(ip, historyClients[i].ipAddr)){
            return 0;
         }else {
             if (i == historyCliCnt -1){
                return -1;
           }
         }
     }
     return -1;
}

//serialize client list
void sortlist(){
    struct clientInfo tmp;
    for (int i = 0; i < logedIncliCnt - 1; i++){
        for (int j = i; j < logedIncliCnt; j++){
            if(atoi(clients[i].portNum) > atoi(clients[j].portNum)){
                tmp = clients[i];
                clients[i] = clients[j];
                clients[j] = tmp;
            }
        }
    }
}

void updateHistoryClients(){
    //first update
    if(historyCliCnt == 0){
        for(int i = 0; i < logedIncliCnt; i++){
            historyClients[i] = clients[i];
            historyCliCnt++;
        }
    }else{
      //check if there is new clients
      for(int i = 0; i < logedIncliCnt; i++){
        for (int j = 0; j < historyCliCnt; j++){
          if(!strcmp(clients[i].ipAddr, historyClients[j].ipAddr)){
            break;
          }else if (j == historyCliCnt - 1){
            historyClients[j+1] = clients[i];
            historyCliCnt++;
            break;
          }
        }
      }
    }
}

void serializeclientList() {
  char *client;
  char *clienttmp[4];  //store the user info that seperate
  char *property;
  int j;

  client = strtok(clientList, " ");
  logedIncliCnt = 0;
  //handle user
  while (client != NULL) {
    clienttmp[logedIncliCnt] = malloc(60);
    strcpy(clienttmp[logedIncliCnt], client);
    client = strtok(NULL, " ");
    logedIncliCnt++;
  }

  for(int i = 0; i < logedIncliCnt; i++){
      memset(&property, 0, sizeof(property));
      property = strtok(clienttmp[i], "_");
      j = 0;
      memset(&clients[i], 0, sizeof(struct clientInfo));
      while (property != NULL) {
          //when log in, check if there is buffer msg, it is append behind the msg
          switch (j) {
            case 0:
                strcpy(clients[i].hostname, property);
                break;
            case 1:
                strcpy(clients[i].ipAddr, property);
                break;
            case 2:
                strcpy(clients[i].portNum, property);
                break;
            default:
                break;
          }
          j++;
          property = strtok(NULL, "_");
      }
  }
  if (historyCliCnt < 4){
    updateHistoryClients();
  }
  sortlist();
}

//command handler
int logIn(char* server, char* port, char* localPort){
    //socket params
    int status;
    struct addrinfo hints, *servinfo;
    struct sockaddr_in local;
    struct sockaddr_in sa;
    int yes = 1;

    //the log-in user list that returns
    int nbytes;
    //local addr
    memset(&local, 0, sizeof(struct sockaddr_in));
    local.sin_family = AF_INET;
    local.sin_port = htons(atoi(localPort));
    local.sin_addr.s_addr = INADDR_ANY;

    //check if ip is vaild
    if (inet_pton(AF_INET, server, &(sa.sin_addr))  <= 0){
       // printf("ip is not vaild\n");
       return -1;
    }

    //check if port is number
    for (int i = 0; i < strlen(port); i++){
        if(port[i] < '0' || port[i] > '9'){
          // printf("port is not vaild\n");
          return -1;
        }
    }

    //  load up address structs with getaddrinfo():
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(server, port, &hints, &servinfo)) == -1){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    // make a socket:
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1){
        perror("client: fail to create socket");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    // if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) == -1) {
    //   perror("setsockopt");
    //   exit(1);
    // }
    //

    //bind local port
    if(bind(sockfd, (struct sockaddr *)&local, sizeof(struct sockaddr)) == -1){
        // cse4589_print_and_log("bind local port failed\n");
        return -1;
    }

    // connect!
    if(connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1){
        close(sockfd);
        perror("client: fail to connect");
        return -1;
    }

    logined = 1;
    FD_SET(sockfd, &clientMasterfd);
    //keep track of the biggest file descriptor
    clientfdmax = sockfd > STDIN ? sockfd : STDIN;

    return 0;
}

int sendMessage(char* clientIP, char* message){
    int res = checkIPVaild(clientIP);
    if(res == -1){
        return -1;
    }
     //check if message is exceed maximum length
     int length, bytes_sent;
     length = strlen(message);
     if(length > 256){
         // cse4589_print_and_log("[message exceed maximum length:ERROR]\n");
         return -1;
     }

     char msg[300];
     memset(&msg, 0, sizeof msg); //make sure the struct is empty

     strcat(msg, "SEND");
     strcat(msg, " ");
     strcat(msg, clientIP);
     strcat(msg, " ");
     strcat(msg, message);

     length = strlen(msg);

     bytes_sent = send(sockfd, msg, length, 0);

    return 0;
}

int refresh(){
  int nbytes;
  int length, bytes_sent;
  length = strlen("REFRESH");
  bytes_sent = send(sockfd, "REFRESH", length, 0);

 return 0;
}

int boardcast(char *message){
  int length, bytes_sent;
  length = strlen(message);
  if(length > 256){
      // cse4589_print_and_log("[message exceed maximum length:ERROR]\n");
      return -1;
  }
  char msg[280];
  memset(&msg, 0, sizeof msg); //make sure the struct is empty

  strcat(msg, "BROADCAST");
  strcat(msg, " ");
  strcat(msg, message);

  length = strlen(msg);

  bytes_sent = send(sockfd, msg, length, 0);

 return 0;
}

//block and unblock
int block(char *ipAddr){
    int res = checkIPVaild(ipAddr);
    if(res == -1){
        return -1;
    }
    //check if is already blocked
    for (int i = 0; i < 4; i++){
        if (blockList[i] != NULL && !strcmp(blockList[i], ipAddr)){
            // cse4589_print_and_log("IP has aleady been in block list%s\n", ipAddr);
            return -1;
        }
    }

    char msg[60];
    memset(&msg, 0, sizeof msg); //make sure the struct is empty
    strcat(msg, "BLOCK");
    strcat(msg, " ");
    strcat(msg, ipAddr);

    int length = strlen(msg);
    int bytes_sent = send(sockfd, msg, length, 0);

    //update local list
    for (int i = 0; i < 4; i++){
        if (blockList[i] == NULL){
            blockList[i] = malloc(20);
            strcpy(blockList[i], ipAddr);
            blockcnt++;
            break;
        }
    }
  return 0;
}

int unblock(char *ipAddr){
  int res = checkIPVaild(ipAddr);
  if(res == -1){
    return -1;
  }

  //update local list
  for (int i = 0; i < 4; i++){
      if(blockList[i] != NULL && !strcmp(blockList[i], ipAddr)){
          memset(&blockList[i], 0, sizeof blockList[i]); //remove
          blockcnt--;
          //send to server
          char msg[60];
          memset(&msg, 0, sizeof msg); //make sure the struct is empty
          strcat(msg, "UNBLOCK");
          strcat(msg, " ");
          strcat(msg, ipAddr);
          int length = strlen(msg);
          int bytes_sent = send(sockfd, msg, length, 0);
          return 0;
      }
  }
  // cse4589_print_and_log("IP does not in block list%s\n", ipAddr);
  return -1;
}
//main function
int runAsClient(char* port) {

  //clean the master and temp sets
  FD_ZERO(&clientMasterfd);
  FD_ZERO(&clientRead_fds);
  int i;
  //add the stdin to the master sets
  FD_SET(STDIN, &clientMasterfd);
  struct timeval tv;

  /* Wait up to five seconds. */
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  //ensure the memory of block list is clean
  memset(&blockList, 0 ,sizeof(blockList));
  blockcnt = 0;

  //revcevied msg
  char msgrec[300];
  int nbytes;
  //command loop
  while(1){
    clientRead_fds = clientMasterfd; //copy it
    //add the listener to the master sets

    if (select(clientfdmax + 1, &clientRead_fds, NULL, NULL, &tv) == -1){
      perror("select");
      exit(4);
    }

    if(FD_ISSET(STDIN, &clientRead_fds)){
      //input value
      int c;
      char result[CMDLENTH];
      int position = 0;
      //handle characters
      while(1){
          c = getchar();
          if(c == EOF || c == '\n'){
            result[position] = '\0';
            break;
          }else{
            result[position] = c;
          }
          position++;
      }
      //seperate cmd para
      char *cmd;
      char *argv[4];
      //handle command
      cmd = strtok(result, " ");
      int i = 0;
      while (cmd != NULL && i <= 2) {
        /*if the command contains message, strtok may divide the message
          need to handle the command paras by command
          command that conatins paras:
          SEND<ip><msg>, BROADCAST<msg>
          msg may conatins " ", so it need to handle seperately, also the memory alloate need be 256
          LOGIN<ip><port>, BLOCK<ip>, UNBLOCK<ip>
          below paras does not contains " ", so it can be handled
        */
        //alloc memory
        if (i > 0 && !(strcmp(argv[0], "SEND"))){
            if(i == 1){
                argv[i] = malloc(20);
            }else if(i == 2){
                argv[i] = malloc(256);
            }
        }
        else if (i > 0 && !(strcmp(argv[0], "BROADCAST"))){
            argv[i] = malloc(256);
        }
        else{
            argv[i] = malloc(20);
        }
        /**********     *********/
        //handle input
        if (i == 2 && !(strcmp(argv[0], "SEND"))) {
            strcpy(argv[i], cmd);
            cmd = strtok(NULL, " ");

            while (cmd != NULL){
                strcat(argv[i], " ");
                strcat(argv[i], cmd);
                cmd = strtok(NULL, " ");
            }
        }
        else if (i == 1 && !(strcmp(argv[0], "BROADCAST"))) {
              strcpy(argv[i], cmd);
              cmd = strtok(NULL, " ");

              while (cmd != NULL){
                  strcat(argv[i], " ");
                  strcat(argv[i], cmd);
                  cmd = strtok(NULL, " ");
              }
        }
        else {
            strcpy(argv[i], cmd);
            cmd = strtok(NULL, " ");
        }
        i++;
      }

      //process command
      if (!(strcmp(argv[0], "AUTHOR"))){
          cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
          cse4589_print_and_log("I, hwang67, have read and understood the course academic integrity policy.\n");
          cse4589_print_and_log("[%s:END]\n", argv[0]);

      }
      else if (!(strcmp(argv[0], "IP"))){
          if(getOwnIP() == 0){
              cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
              cse4589_print_and_log("IP:%s\n", localIP);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "PORT"))){
          cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
          cse4589_print_and_log("PORT:%s\n", port);
          cse4589_print_and_log("[%s:END]\n", argv[0]);
      }
      else if (!(strcmp(argv[0], "LIST"))){
          if(logined == 1){
              cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
              for (int k = 0; k < logedIncliCnt; k++){
                  cse4589_print_and_log("%-5d%-40s%-20s%-8s\n", k+1, clients[k].hostname, clients[k].ipAddr, clients[k].portNum);
              }
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }else{
            cse4589_print_and_log("[%s:ERROR]\ren", argv[0]);
            cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "LOGIN"))){
          // cse4589_print_and_log("IP:%s, PORT:%s\n",argv[1], argv[2]);
          if(logined == 0 && argv[1] != NULL && argv[2] != NULL){
              if (logIn(argv[1], argv[2], port) == 0){
                  cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                  cse4589_print_and_log("[%s:END]\n", argv[0]);
              }else{
                  cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
                  cse4589_print_and_log("[%s:END]\n", argv[0]);
              }
          }else{
                cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
                cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "REFRESH"))){
          if(logined == 1){
              cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
              refresh();
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }else{
              cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "SEND"))){
          if(logined == 1 && argv[1] != NULL && argv[2] != NULL){
            if (sendMessage(argv[1], argv[2]) == 0){
                cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                cse4589_print_and_log("[%s:END]\n", argv[0]);
            }else{
                cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
                cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
          }else{
              cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "BROADCAST"))){
          if(logined == 1 && argv[1] != NULL){
            if(boardcast(argv[1]) == 0){
              cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
            }else{
              cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
          }else{
            cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
            cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "BLOCK"))){
          if(logined == 1 && argv[1] != NULL){
              if(block(argv[1]) == 0){
                  cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                  cse4589_print_and_log("[%s:END]\n", argv[0]);
              }else{
                  cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
                  cse4589_print_and_log("[%s:END]\n", argv[0]);
              }
          }else{
              cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "UNBLOCK"))){
          if(logined == 1 && argv[1] != NULL){
              if(unblock(argv[1]) == 0){
                  cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                  cse4589_print_and_log("[%s:END]\n", argv[0]);
              }else{
                  cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
                  cse4589_print_and_log("[%s:END]\n", argv[0]);
              }
          }else{
              cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "LOGOUT"))){
          if (logined == 1){
              cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
              close(sockfd);
              FD_CLR(sockfd, &clientMasterfd); // remove from master set
              logined = 0;
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }else{
              cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
          }
      }
      else if (!(strcmp(argv[0], "EXIT"))){
          cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
          cse4589_print_and_log("[%s:END]\n", argv[0]);
          if(logined == 1){
              close(sockfd);
              FD_CLR(sockfd, &clientMasterfd); // remove from master set
          }
          logined = 0;
          exit(0);
      }
      else{
        cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
        cse4589_print_and_log("[%s:END]\n", argv[0]);
      }

    }else{
      //run through the existing connections looking for data to read
          if(FD_ISSET(sockfd, &clientRead_fds)){
                memset(&msgrec, 0, sizeof(msgrec));
                if((nbytes = recv(sockfd, msgrec, sizeof msgrec, 0)) <= 0){
                    //got error or connection closed by client
                    if (nbytes == 0){
                        //connection closed
                        // cse4589_print_and_log("server disconnected\n");

                        for (int i = 0; i < 4; i++){
                          memset(&blockList[i], 0, sizeof blockList[i]); //remove
                        }
                        logined = 0;

                    }else{
                        perror("recv");
                    }
                    close(sockfd);
                    FD_CLR(sockfd, &clientMasterfd); // remove from master set
                }else{
                      //coming event handler
                      // cse4589_print_and_log("msg recv:%s\n", msgrec);
                      char *msghandler;

                      //check if it is buffer msg, start with *
                      if(msgrec[0] == '*'){
                          msghandler = strtok(msgrec, "*:");
                          // cse4589_print_and_log("handler:%s\n", msghandler);
                          while(msghandler != NULL){
                                char ip[20], msg[256];
                                //handle client list
                                if(!strcmp(msghandler,"LIST")){
                                  msghandler = strtok(NULL, ":");
                                  strcpy(clientList, msghandler);
                                  serializeclientList();
                                  break;
                                }

                                //get IP
                                msghandler = strtok(NULL, "*:");
                                // cse4589_print_and_log("handler:%s\n", msghandler);
                                strcpy(ip, msghandler);
                                //get message context
                                msghandler = strtok(NULL, "*:");
                                // cse4589_print_and_log("handler:%s\n", msghandler);
                                strcpy(msg, msghandler);

                                cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
                                cse4589_print_and_log("msg from:%s\n[msg]:%s\n", ip, msg);
                                cse4589_print_and_log("[RECEIVED:END]\n");

                                msghandler = strtok(NULL, "*:");
                          }
                      }

                      msghandler = strtok(msgrec, ":");
                      //two coming msg types: list and msg
                      if (!strcmp(msghandler, "LIST")) {
                          msghandler = strtok(NULL, ":");
                          strcpy(clientList, msghandler);
                          serializeclientList();
                      }
                      else if(!strcmp(msghandler, "MESSAGE")){
                          //tokenize the data, seperate the msg
                          char ip[20], msg[256];

                          msghandler = strtok(NULL, ":");
                          strcpy(ip, msghandler);
                          msghandler = strtok(NULL, ":");
                          strcpy(msg, msghandler);
                          cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
                          cse4589_print_and_log("msg from:%s\n[msg]:%s\n", ip, msg);
                          cse4589_print_and_log("[RECEIVED:END]\n");
                      }
                }

            }
      }
  }
  return 0;
}
