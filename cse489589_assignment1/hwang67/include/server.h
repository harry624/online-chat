//client info struct
struct hisClientInfo{
  char hostname[40];
  char hostIP[20];
  char portNum[6];
  char *blockList[4];
  int islogIn;
  int msgsent;
  int msgrecv;
  char *bufferedmsg[100];
};

int runAsServer(char* port);
