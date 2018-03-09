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
#include <time.h>

#include "../include/global.h"
#include "../include/logger.h"
#include "../include/client.h"
#include "../include/server.h"
#define BACKLOG 5

char serverIP[20];


struct hisClientInfo hisClients[4];
int hisClientscnt;

int listenfd;                         // listen on listenfd
struct sockaddr_storage remoteaddr;  //conntecter's address information;
socklen_t sin_size;

fd_set master;    //master file discriptor list
fd_set read_fds;  //temp file discriptor list for select()
int fdmax;        //maximum file descriptor number

int length, bytes_sent;
char userList[512];

void sort(){
    struct hisClientInfo tmp;
    for (int i = 0; i < hisClientscnt - 1; i++){
        for (int j = i; j < hisClientscnt; j++){
            if(atoi(hisClients[i].portNum) > atoi(hisClients[j].portNum)){
              tmp = hisClients[i];
              hisClients[i] = hisClients[j];
              hisClients[j] = tmp;
            }
        }
    }
}
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET){
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int getServerIP(){
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
        return -1;
    }
    //connect
    if (connect(getIPsockfd, res -> ai_addr, res -> ai_addrlen) == -1){
        close(getIPsockfd);
        perror("fail to connect");
        return -1;
    }
    struct sockaddr_in localAddr;
    addr_len = sizeof localAddr;

    if (getsockname(getIPsockfd, (struct sockaddr *)&localAddr, &addr_len) == -1){
        perror("fail to get sock name");
        return -1;
    }
    //turn address struct to dot notation
    const char *p = inet_ntop(AF_INET, &localAddr.sin_addr, serverIP, sizeof(serverIP));
    close(getIPsockfd);

    // cse4589_print_and_log("IP:%s\n", serverIP);

    return 0;
}

//check ip vaildiation
int checkIPVaildation(char *ip){
    //check if ip is vaild
    struct sockaddr_in sa;
    int res = inet_pton(AF_INET, ip, &(sa.sin_addr));
    if (res < 1){
        printf("ip address is invaild\n");
        return -1;
    }
     //check if it is the host
     if (serverIP[0] == '\0' ){
        getServerIP();
     }

     if (!strcmp(ip, serverIP)){
        printf("ip is the server ip\n");
        return -1;
     }

     //check if the ip exist in the local copy of list
     for(int i = 0; i < 4; i++){
         if(!strcmp(ip, hisClients[i].hostIP)){
            return 0;
         }
     }
     printf("ip address is not on the list\n");
     return -1;
}

//return the update client list
void *updateClientList(){
  sin_size = sizeof remoteaddr;
  char ipstr[INET6_ADDRSTRLEN];
  int port_num;
  char hostname[50];
  //copy the current user list to char
  strcat(userList, "LIST:");
  for (int k = 1; k <= fdmax; k++){
    //check if fd is in the master list
      if (FD_ISSET(k, &master)){
          if (k == listenfd){
            continue;
          }
          //get ip addr and port
          getpeername(k, (struct sockaddr *)&remoteaddr, &sin_size);
          if (remoteaddr.ss_family == AF_INET){
            struct sockaddr_in *addr = (struct sockaddr_in *)&remoteaddr;
            port_num = ntohs(addr->sin_port);
            inet_ntop(AF_INET, &addr->sin_addr, ipstr, sizeof ipstr);
            //get host name
            getnameinfo((struct sockaddr *)&remoteaddr, sin_size, hostname, sizeof hostname, NULL, 0, 0);
            //handle hostname, ipstr, portnum
            char port_s[6];
            sprintf(port_s, "%d", port_num);
            strcat(hostname, "_");
            strcat(hostname, ipstr);
            strcat(hostname, "_");
            strcat(hostname, port_s);
            strcat(userList, hostname);
            strcat(userList, " ");
          }
      }
  }
  return 0;
}

//show Statistics list
int showStatistics(){
    sort();

    for (int i = 0; i < hisClientscnt; i++){
        char status[10];
        if (hisClients[i].islogIn == 1){
            strcpy(status, "logged-in");
        }else{
            strcpy(status, "logged-out");
        }
        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", i+1,
                              hisClients[i].hostname, hisClients[i].msgsent,
                              hisClients[i].msgrecv, status);
    }
    return 0;
}

//show block list of ip
int showblockList(char* ipAddr){
    //get the host ip form the list
    int k;
    for(k = 0; strcmp(hisClients[k].hostIP, ipAddr) && k < 4; k++);
    if (k >= 4){
        printf("ip not in the list\n");
        return -1;
    }

    //check
    if (checkIPVaildation(ipAddr) == -1){
        printf("ip invaild\n");
        return -1;
    }

    int id = 0;
    struct hisClientInfo displayList[4];
    //copy
    for(int i = 0; hisClients[k].blockList[i] != NULL && i < 4; i++){
        for(int j = 0; j < 4; j++){
            if(!strcmp(hisClients[k].blockList[i], hisClients[j].hostIP)){
                displayList[id] = hisClients[j];
                id++;
            }
        }
    }
    //sort
    struct hisClientInfo tmp;
    for (int i = 0; i < id - 1; i++){
        for (int j = i; j < id; j++){
            if(atoi(displayList[i].portNum) > atoi(displayList[j].portNum)){
              tmp = displayList[i];
              displayList[i] = displayList[j];
              displayList[j] = tmp;
          }
        }
    }
    //print
    for (int i = 0; i < id; i++) {
        cse4589_print_and_log("%-5d%-40s%-20s%-8s\n", i+1, displayList[i].hostname, displayList[i].hostIP, displayList[i].portNum);
    }

    return 0;
}

//main function
int runAsServer(char* port) {
    for (int i = 0; i < 4; i++){
      memset(&hisClients[i], 0, sizeof(struct hisClientInfo));
    }
    //CREATE SOCKET
    int status;
    int new_fd;  // new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];   //incoming connection ip

    //select() paras
    fd_set allUsers;   //history users
    struct timeval tv;
    int i, j;

    char buf[512]; //buffer for client data
    int nbytes;

    FD_ZERO(&master);     //clean the master and temp sets
    FD_ZERO(&read_fds);

    /* Wait up to three seconds. */
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    memset(&hints, 0, sizeof hints); //make sure the struct is empty
    hints.ai_family = AF_UNSPEC; //don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; //fill in my ip for me

    //get address info
    if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }
    //loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p -> ai_next){
        if ((listenfd = socket(p ->ai_family, p -> ai_socktype, p ->ai_protocol)) == -1){
            perror("server: socket");
            continue;
        }
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("setsockopt");
            exit(1);
        }
        //bind the port
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == -1){
            close(listenfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); //add done with this structure

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n" );
        exit(1);
    }
    if(listen(listenfd, BACKLOG) == -1){
        perror("listen");
        exit(1);
    }
    cse4589_print_and_log("[server started: waiting for connections...]\n");
    //add the listener to the master sets
    FD_SET(listenfd, &master);
    //add the stdin to the master sets
    FD_SET(STDIN, &master);

    //keep track of the biggest file descriptor
    fdmax = listenfd > STDIN ? listenfd : STDIN;

    // //main accpet loop
    while(1){
        read_fds = master; //copy it
        if (select(fdmax + 1, &read_fds, NULL, NULL, &tv) == -1){
            perror("select");
            exit(4);
        }

        if(FD_ISSET(STDIN, &read_fds)){
            //input value
            int c;
            char result[CMDLENTH];
            int position = 0;
            //handle characters
            while(1){
              c = getchar();
              if(c == EOF || c == '\n'){
                result[position] = '\0';
                // cse4589_print_and_log("EOF\n");
                break;
              }else{
                // cse4589_print_and_log("%c\n", c);
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
            while (cmd != NULL) {
              argv[i] = malloc(20);
              strcpy(argv[i], cmd);
              cmd = strtok(NULL, " ");
              i++;
            }

            if (!(strcmp(argv[0], "AUTHOR"))){
                cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                cse4589_print_and_log("I, hwang67, have read and understood the course academic integrity policy.\n");
                cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
            else if (!(strcmp(argv[0], "IP"))){
                if (getServerIP() == 0){
                    cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                    cse4589_print_and_log("IP:%s\n", serverIP);
                    cse4589_print_and_log("[%s:END]\n", argv[0]);
                }else{
                    cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
                    cse4589_print_and_log("[%s:END]\n", argv[0]);
                }

            }
            else if (!(strcmp(argv[0], "PORT"))){
                cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                cse4589_print_and_log("PORT:%s\n", port);
                cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
            else if (!(strcmp(argv[0], "LIST"))){
                    cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                    // sin_size = sizeof remoteaddr;
                    // char ipstr[INET6_ADDRSTRLEN];
                    // int port_num;
                    // char hostname[50];
                    //
                    // for (int k = 1; k <= fdmax; k++){
                    //       //check if fd is in the master list
                    //       if (FD_ISSET(k, &master)){
                    //           if (k == listenfd){
                    //               continue;
                    //           }
                    //           //get ip addr and port
                    //           int n = 1;
                    //           getpeername(k, (struct sockaddr *)&remoteaddr, &sin_size);
                    //           if (remoteaddr.ss_family == AF_INET){
                    //                 struct sockaddr_in *addr = (struct sockaddr_in *)&remoteaddr;
                    //                 port_num = ntohs(addr->sin_port);
                    //                 inet_ntop(AF_INET, &addr->sin_addr, ipstr, sizeof ipstr);
                    //                 //get host name
                    //                 getnameinfo((struct sockaddr *)&remoteaddr, sin_size, hostname, sizeof hostname, NULL, 0, 0);
                    //                 cse4589_print_and_log("%-5d%-40s%-20s%-8d\n", n, hostname, ipstr, port_num);
                    //                 n++;
                    //           }
                    //       }
                    // }

                    sort();
                    int id = 1;
                    for (int i = 0; i < hisClientscnt; i++){
                        if (hisClients[i].islogIn == 1){
                            cse4589_print_and_log("%-5d%-40s%-20s%-8s\n", id,
                                                  hisClients[i].hostname, hisClients[i].hostIP,
                                                  hisClients[i].portNum);
                            id++;
                        }
                    }
                    cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
            else if (!(strcmp(argv[0], "STATISTICS"))){
              cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
              showStatistics();
              cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
            else if (!(strcmp(argv[0], "BLOCKED"))){
                cse4589_print_and_log("[%s:SUCCESS]\n", argv[0]);
                showblockList(argv[1]);
                cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
            else{
              cse4589_print_and_log("[%s:ERROR]\n", argv[0]);
              cse4589_print_and_log("[%s:END]\n", argv[0]);
            }
        }else{
          //run through the existing connections looking for data to read
          for (i = 0; i <= fdmax; i++){
            if(FD_ISSET(i, &read_fds)){

              if (i == listenfd) {
                    //handle new connections
                    sin_size = sizeof remoteaddr;
                    new_fd = accept(listenfd, (struct sockaddr *)&remoteaddr, &sin_size);
                    if(new_fd == -1){
                          perror("accept");
                          continue;
                    }else{
                          FD_SET(new_fd, &master); //add to the master set
                          FD_SET(new_fd, &allUsers); //save it to all users set

                          if (new_fd > fdmax){
                            fdmax = new_fd;
                          }
                          //list the master lists

                          //add user in the history list
                          char hostname[50];
                          getpeername(new_fd, (struct sockaddr *)&remoteaddr, &sin_size);
                          struct sockaddr_in *addr = (struct sockaddr_in *)&remoteaddr;
                          int port_num = ntohs(addr->sin_port);
                          inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr *)&remoteaddr), s, sizeof s);
                          cse4589_print_and_log("server: got connection from %s\n", s);
                          //get host name
                          getnameinfo((struct sockaddr *)&remoteaddr, sin_size, hostname, sizeof hostname, NULL, 0, 0);
                          char port_s[6];
                          sprintf(port_s, "%d", port_num);
                          //handle hostname, s, portnum
                          int k; //the index of current log in client in hisClients
                          for(k = 0; k < 4; k++){
                              if(strlen(hisClients[k].hostname) > 0){
                                  if(!strcmp(hisClients[k].hostIP, s)){
                                      hisClients[k].islogIn = 1;
                                      strcpy(hisClients[k].portNum, port_s);
                                      break;
                                  }
                              }else{
                                strcpy(hisClients[k].hostname, hostname);
                                strcpy(hisClients[k].hostIP, s);
                                strcpy(hisClients[k].portNum, port_s);
                                hisClients[k].islogIn = 1;
                                hisClients[k].msgsent = 0;
                                hisClients[k].msgrecv = 0;
                                // for (int n = 0; n < 100; n++){
                                //   memset(&hisClients[k].bufferedmsg[n], 0, sizeof(hisClients[k].bufferedmsg[n]));
                                // }
                                hisClientscnt++;
                                break;
                              }
                          }
                          /*
                            check if there is buffer msg first, and send back to client,
                            then use nanosleep to pause the process for 0.5s
                            so that the client update list will send seperately
                          */
                          if(hisClients[k].bufferedmsg[0] != NULL){
                              for (int n = 0; hisClients[k].bufferedmsg[n] != NULL && n < 100; n++){
                                  // cse4589_print_and_log("send buffer msg:%s\n", hisClients[k].bufferedmsg[n]);
                                  char *msghandler;
                                  msghandler = strtok(hisClients[k].bufferedmsg[n], ":*");
                                  char ip[20], msg[256];
                                  msghandler = strtok(NULL, "*:");
                                  strcpy(ip, msghandler);
                                  msghandler = strtok(NULL, "*:");
                                  strcpy(msg, msghandler);

                                  length = strlen(hisClients[k].bufferedmsg[n]);
                                  bytes_sent = send(new_fd, hisClients[k].bufferedmsg[n], length, 0);
                                  if(bytes_sent == length){
                                      cse4589_print_and_log("[RELAYED:SUCCESS]\n");
                                      cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", ip, hisClients[k].hostIP, msg);
                                      cse4589_print_and_log("[RELAYED:END]\n");
                                      //after sending the msg, need to clean the buffer
                                      hisClients[k].bufferedmsg[n] = NULL;
                                      hisClients[k].msgrecv++;
                                      // cse4589_print_and_log("msg %d succeed\n", n);
                                  }
                              }

                              //List of all currently logged-in clients
                              memset(&userList, 0, sizeof userList);
                              updateClientList();
                              char userlistsent[520];
                              strcpy(userlistsent, "*");
                              strcat(userlistsent, userList);
                              //send back the client list to client
                              length = strlen(userlistsent);
                              bytes_sent = send(new_fd, userlistsent, length, 0);

                          }else{
                            //List of all currently logged-in clients
                            memset(&userList, 0, sizeof userList);
                            updateClientList();

                            //send back the client list to client
                            length = strlen(userList);
                            bytes_sent = send(new_fd, userList, length, 0);
                          }


                    }
              }else{
                  //handle data from i, i is not the listenfd
                  /* i */
                  //get the ip addr of fd i
                    struct sockaddr_in addr;
                    socklen_t addr_size = sizeof(struct sockaddr_in);
                    int res = getpeername(i, (struct sockaddr *)&addr, &addr_size);
                    char senderIP[20];
                    strcpy(senderIP, inet_ntoa(addr.sin_addr));
                    // cse4589_print_and_log("senderIP:%s\n", senderIP);
                    memset(&buf, 0, sizeof(buf));

                    if((nbytes = recv(i, buf, sizeof buf, 0)) <= 0){
                        //got error or connection closed by client
                        if (nbytes == 0){
                            //connection closed
                            cse4589_print_and_log("selectserver: socket %d log out, IP:%s\n", i, senderIP);
                            //update the status
                            for(int i = 0; i < hisClientscnt; i++){
                                if(!strcmp(hisClients[i].hostIP, senderIP)){
                                    hisClients[i].islogIn = 0;
                                    break;
                                }
                            }
                            close(i);             //close fd
                            FD_CLR(i, &master);   //move from master set
                        }else{
                          perror("recv");
                        }
                    }else{
                          //we got some NEW msg from a client

                          //tokenize the data, check which command it is
                          char *cmd;
                          //get command
                          cmd = strtok(buf, " ");

                          if (!(strcmp(cmd, "REFRESH"))){
                            memset(&userList, 0, sizeof userList);
                            updateClientList();
                            length = strlen(userList);
                            // cse4589_print_and_log("%s\n", userList);
                            bytes_sent = send(i, userList, length, 0);
                          }
                          else if (!(strcmp(cmd, "SEND"))){
                            char destIP[20], msg[256];
                            memset(&msg, 0 ,sizeof(msg));
                              //get ip
                              cmd = strtok(NULL, " ");
                              strcpy(destIP, cmd);
                              //get msg
                              cmd = strtok(NULL, "");
                              strcpy(msg, cmd);
                              //STATISTICS count
                              for(int i = 0; i < hisClientscnt; i++){
                                  if(!strcmp(hisClients[i].hostIP, senderIP)){
                                      hisClients[i].msgsent++;
                                  }
                              }


                              //handle msg:
                              char msgsent[300];
                              memset(&msgsent, 0, sizeof(msgsent));
                              strcat(msgsent, "MESSAGE:");
                              strcat(msgsent, senderIP);
                              strcat(msgsent, ":");
                              strcat(msgsent, msg);

                              nbytes = strlen(msgsent);
                              bytes_sent = 0;
                              int isBlocked = 0;

                              //check if ip is log-in, if is, send it to the destion
                              for (j = 1; j <= fdmax; j++){
                                    if (FD_ISSET(j, &master)){
                                        //not listener and not self
                                        if (j != listenfd && j != i){
                                            //find the destination ip
                                            struct sockaddr_in addr;
                                            socklen_t addr_size = sizeof(struct sockaddr_in);
                                            int res = getpeername(j, (struct sockaddr *)&addr, &addr_size);
                                            if(!strcmp(destIP, inet_ntoa(addr.sin_addr))){
                                                //check if it is in the block list of receiver
                                                int k;
                                                for(k = 0; strcmp(destIP, hisClients[k].hostIP) && k < 4; k++);
                                                // cse4589_print_and_log("find socket: %d, destIP:%s\n", j, hisClients[k].hostIP);
                                                for(int m = 0; (hisClients[k].blockList[m] != NULL) &&m < 4; m++){
                                                    if(!strcmp(hisClients[k].blockList[m], senderIP)){
                                                      // cse4589_print_and_log("find block ip:%s\n", hisClients[k].blockList[m]);
                                                      isBlocked = 1;
                                                    }
                                                }
                                                //if not blocked, then send message
                                                if(isBlocked == 0){
                                                    bytes_sent = send(j, msgsent, nbytes, 0);
                                                    //STATISTICS count
                                                    if(bytes_sent == nbytes){
                                                        cse4589_print_and_log("[RELAYED:SUCCESS]\n");
                                                        cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", senderIP, destIP, msg);
                                                        cse4589_print_and_log("[RELAYED:END]\n");
                                                        for(int i = 0; i < hisClientscnt; i++){
                                                            if(!strcmp(hisClients[i].hostIP, destIP)){
                                                                hisClients[i].msgrecv++;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                //not currently log-in, need to buffer
                                //buffer msg format: *MESSAGE:IP:msg
                                if(bytes_sent == 0 && isBlocked == 0){
                                  int k;
                                  int n;
                                  //find the host ip
                                  for(k = 0; strcmp(destIP, hisClients[k].hostIP) && k < 4; k++);
                                  //find the n th empty place to save the buffer msg
                                  for(n = 0; hisClients[k].bufferedmsg[n] != NULL && n < 100; n++);
                                  hisClients[k].bufferedmsg[n] = malloc(300);
                                  strcpy(hisClients[k].bufferedmsg[n], "*");
                                  strcat(hisClients[k].bufferedmsg[n], msgsent);
                                  // cse4589_print_and_log("msg %d buffered, %s\n", n, hisClients[k].bufferedmsg[n]);
                                }
                          }
                          else if (!(strcmp(cmd, "BROADCAST"))){
                                  //sender count
                                  for(int i = 0; i < hisClientscnt; i++){
                                      if(!strcmp(hisClients[i].hostIP, senderIP)){
                                          hisClients[i].msgsent++;
                                      }
                                  }
                                  char msg[256];
                                  memset(&msg, 0 ,sizeof(msg));
                                  cmd = strtok(NULL, "");
                                  strcat(msg, cmd);
                                  cse4589_print_and_log("[RELAYED:SUCCESS]\n");
                                  cse4589_print_and_log("msg from:%s, to:255.255.255.255\n[msg]:%s\n", senderIP, msg);
                                  cse4589_print_and_log("[RELAYED:END]\n");

                                  char msgsent[300];
                                  memset(&msgsent, 0, sizeof(msgsent));
                                  strcat(msgsent, "MESSAGE:");
                                  strcat(msgsent, senderIP);
                                  strcat(msgsent, ":");
                                  strcat(msgsent, msg);

                                  nbytes = strlen(msgsent);

                                  //use FD_SET method
                                  for (j = 1; j <= fdmax; j++){
                                      //check if it is log in
                                      if (FD_ISSET(j, &master)){
                                          if (j != listenfd && j != i){
                                              if(send(j, msgsent, nbytes, 0) == -1){
                                                perror("send");
                                              }
                                          }
                                      }
                                  }
                          }
                          else if (!(strcmp(cmd, "BLOCK"))){
                            int hasBLocked = 0;
                            char blockIP[20];
                              //get ip
                              cmd = strtok(NULL, " ");
                              strcpy(blockIP, cmd);
                              int k;
                              for(k = 0; strcmp(hisClients[k].hostIP, senderIP) && k < 4; k++);

                              //check if the ip has already been blocked
                              for(int i = 0; hisClients[k].blockList[i] != NULL && i < 4; i++){
                                  if (!strcmp(hisClients[k].blockList[i], blockIP)){
                                      // printf("ip already on the list\n");
                                      hasBLocked = 1;
                                      break;
                                  }
                              }

                              //add to list
                              if (hasBLocked == 0){
                                  for (int i = 0; i < 4; i++){
                                      if (hisClients[k].blockList[i] == NULL){
                                          hisClients[k].blockList[i] = malloc(20);
                                          strcpy(hisClients[k].blockList[i], blockIP);
                                          // cse4589_print_and_log("blockIP:%s\n", hisClients[k].blockList[i]);
                                          break;
                                      }
                                  }
                              }


                          }
                          else if (!(strcmp(cmd, "UNBLOCK"))){
                              char unBlockIP[20];
                              //get ip
                              cmd = strtok(NULL, " ");
                              // cse4589_print_and_log("%s\n", cmd);
                              strcpy(unBlockIP, cmd);
                              int k;
                              for(k = 0; strcmp(hisClients[k].hostIP, senderIP) && k < 4; k++);
                              for (int i = 0; i < 4; i++){
                                  if (hisClients[k].blockList[i] != NULL &&!strcmp(hisClients[k].blockList[i], unBlockIP)){
                                      hisClients[k].blockList[i] = NULL;
                                      break;
                                  }
                              }
                          }
                    }
                  }
            }
          }
        }

    }

    return 0;
}
