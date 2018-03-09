//client info struct
struct clientInfo{
  char hostname[40];
  char ipAddr[20];
  char portNum[6];
  // int  isloggedIn;
};

int runAsClient(char* port);
