#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <vector>
#include <fcntl.h>
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
using namespace std;

void error(const char *msg){
  /**
  *error handle function
  */
  perror(msg);
  exit(1);
}


void toserver_output(char* buffer,string client_id,char* filename){

  /**
   * construct out put to server
   * buffer output : Client-ID space file-name
   * @param buffer char array to send in socket
   */
    strcpy(buffer,client_id.c_str());
    strcat(buffer," ");
    strcat(buffer,filename);
    strcat(buffer,"\0");
}

int main(int argc,char* argv[]){

  struct sockaddr_in server_address;
  struct hostent *server;
  int create_socket, port_no;
  string client_id;
  int return_result;// return result holds result from server
  char buffer[256];
  bzero(buffer,256);

  if(argc < 4){
    //print here error
    fprintf(stderr,"usage %s client ID, hostname, port\n", argv[0]);
    exit(0);
  }
  client_id = argv[1];

  port_no = atoi(argv[3]);
  cout<<port_no<<endl;
  /*
  *int socket (int family, int type, int protocol);
  */
  create_socket = socket(AF_INET, SOCK_STREAM, 0);

  if(create_socket <0){
    error("fail openting socket");
  }
  server = gethostbyname(argv[2]);

 if (server == NULL) {
    fprintf(stderr,"fail to find server\n");
    exit(0);
 }

 bzero((char*) &server_address,sizeof(server_address));
 server_address.sin_family = AF_INET;
 bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
 server_address.sin_port = htons(port_no);

 if (connect(create_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
    error("ERROR connecting here in client");
 }
 while(1){
     string line = "";
     string get_line;
     int n = 0;
     char c;
     char read_buffer[256];
    //read input while not read
    //write file name to computer process
    char output_buffer[256];//output_buffer holds msg to write to servevr
    //buffer holds stdin break while \n
     bzero(buffer,256);
     cout<<"enter file name : ";
     cin>>get_line;
     //getline(cin,get_line);

     if(strncmp(get_line.c_str(),"nullfile",7) == 0){
       write(create_socket,"nullfile",7);
       break;
     }
     strcpy(buffer,get_line.c_str());
     toserver_output(output_buffer,client_id,buffer);
     write(create_socket,output_buffer,256);
     get_line = "";
     //output result if read
    return_result = read(create_socket, read_buffer, 256);
    if(return_result < 0){
      error("can not read from server");
    }
    cout<<read_buffer<<endl;
    bzero(read_buffer,256);

  }
}
