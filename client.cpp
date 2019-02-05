#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <regex>
#include <vector>

using namespace std;

void error(const char *msg){
  perror(msg);
  exit(1);
}

bool check_filename(string filename){
  //charactor !@#$%^&*(_ ) are not allowed
  regex format("^[1-9][0-9][0-9]\\s[a-zA-Z\\.]{8}(.*)");
  return regex_match(filename,format);

}
bool check_exit(string exit){
  regex format("nullfile");
  return regex_match(exit,format);
}

void toserver_output(char* buffer,string client_id,char* filename,string client_pn){
  //change to write (Client ID, file name, client socket descriptor, client port number)
  //usage:
  //char buffer[256];
  //toserver_output(buffer,"client_id",filename,"20","client_pn");
    char chararray[50];
    int n = 0;
    strcpy(buffer,client_id.c_str());
    strcat(buffer," ");
    strcat(buffer,filename);
    strcat(buffer," ");
    strcat(buffer,client_pn.c_str());
    strcat(buffer,"\0");
    //cout<<"client output :"<<buffer<<endl;
}

int main(int argc,char* argv[]){

  struct sockaddr_in server_address;
  struct hostent *server;
  int create_socket, port_no;
  string client_id;

  if(argc < 4){
    //print here error
    fprintf(stderr,"usage %s client ID, hostname, port\n", argv[0]);
    exit(0);
  }
  client_id = argv[1];

  port_no = atoi(argv[3]);
  cout<<port_no<<endl;
  /*
  int socket (int family, int type, int protocol);
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
 //send_conn_msg(create_socket,client_id);

 while(1){
   string line = "";
   char buffer[50];
   bzero(buffer,50);
   int n = 0;
   char c;

   cout<<"enter file name :"<<endl;
   while ((c = getchar()) != '\n'){
     buffer[n++] = c;
   }
   //getline(cin,line);
   line = string(buffer,n);

   if(check_exit(line)){
     write(create_socket,"client exit",12);
     break;
   }
   /*
   else if(check_filename(line)!=1){
     cout<<"wrong file name format"<<endl;
     cout<<"length:"<<sizeof(line)<<endl;
     cout<<line<<endl;
   }*/
   //else if(read(create_socket,buffer,256)>0){
     //if reasult is ready, read result
     //cout<<buffer<<endl;
     //bzero(buffer,50);
   //}
   else{
  //change to write (Client ID, file name, client socket descriptor, client port number)
  char output_buffer[256];
   toserver_output(output_buffer,client_id,buffer,argv[3]);
   write(create_socket,output_buffer,256);
   bzero(buffer, 50);
   read(create_socket, buffer, 50);
   cout<<"server :"<< buffer<<endl;
   bzero(output_buffer,256);
 }
}
close(create_socket);//close socket when exit client
exit(0);
}
