#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <netdb.h>
#include <netinet/in.h>
#include <fstream>
#include <sys/time.h>
#include <pthread.h>
#include <queue>
#include <sstream>
#include <vector>
#include <signal.h>

using namespace std;

int fds1[2], fds2[2];

struct clientset{

  int socket_sd;

};

struct result_data{
  string client_id;
  string client_sd;
  string file_name;
  int sum;
};

typedef struct {
    string client_id;
    string file_name;
    string client_sd;
    string client_port_num;
}readyqueue_data;

struct pipe_param{

  int c_read;
  int c_write;

} ;

struct socket_param{
    int port_num;
    //queue<readyqueue_data> readyqueue;
};

void error(const char *msg){
  perror(msg);
  exit(1);
}

queue<result_data> summning_queue; //try global variable
queue<readyqueue_data> readyqueue;
vector<int> client_set;
useconds_t sleep_time;

fd_set socket_fdset;//set of socket descriptors

string trim_input(string str) {
    str.erase(remove(str.begin(), str.end(), '\n'), str.end());
    return str;
}

void signal_handler(int sig_num){
  //when receive Q from admin Dump ready queue content
  cout<<"dump all requests in ready queue"<<endl;
  cout<<"request in queue: "<<endl;
  cout<<"client ID"<<" "<<"file name"<<" "<<"client SD"<<" "<<"client port num"<<endl;
  while(!readyqueue.empty()){
    readyqueue_data front = readyqueue.front();
    readyqueue.pop();
    cout<<front.client_id<<" "<<front.file_name<<" "<<front.client_sd<<" "<<front.client_port_num<<endl;
  }
}

readyqueue_data create_queue_in(string client_id,string file_name,string client_sd,string client_port_num){
  readyqueue_data row;
  row.client_id = client_id;
  row.file_name = trim_input(file_name);
  row.client_sd = client_sd;
  row.client_port_num = client_port_num;
  return row;
}

readyqueue_data parse_msg(char* buffer,int socket){
  //parse msg to add to ready queue
  //parse_msg(buffer,&row);
  int n = 1;
  string line = string(buffer),word;
  stringstream ss(line);
  vector<string> split;
  while(getline(ss, word,' ')) {
    split.push_back(word);
  }
  return create_queue_in(split[0],split[1],to_string(socket),split[3]);
}

void send_result(){
  result_data row;
  char buffer[50];
  string client_id;
  string client_sd;
  string file_name;
  int sum;
  while(!summning_queue.empty()){
    bzero(buffer,50);
    row = summning_queue.front();
    summning_queue.pop();
    client_id = row.client_id;
    client_sd = row.client_sd;
    file_name = row.file_name;
    sum = row.sum;
    strcpy(buffer,client_id.c_str());
    strcat(buffer," ,");
    strcat(buffer,file_name.c_str());
    strcat(buffer," ,");
    strcat(buffer,to_string(sum).c_str());
    write(stoi(client_sd),buffer,sizeof(buffer));//now to admin should be in socket(to client)
  }
}

void computer_process(int socket){
  //in socket thread
  char buffer[256];
  int read_msg;
  bzero(buffer,256);

  read_msg = read(socket,buffer,256);
  if(read_msg<0){
    error("fail to read from client");
  }
  cout<<"client message : "<<buffer<<endl;
  readyqueue_data row = parse_msg(buffer,socket);
  readyqueue.push(row);
  if(readyqueue.empty()){
    error("fail to put in queue, in soclet thread");
  }
  read_msg = write(socket,"message received :",16);

  if(read_msg<0){
    error("fail to response to client");
  }
}

int sum(int M,int file[]){
  int sum = 0;
  for(int i = 0;i < M;i++){
    sum += file[i];
    usleep(sleep_time);
  }
  return sum;
}

int sum_file(string file_name){

    ifstream myfile;
    //filestruct data_out;
    cout<<"file name :"<<file_name<<endl;
    myfile.open(file_name);
    if(!myfile.is_open()){
      error("file not found");
    }
    int M;
    myfile >> M; // first line
    int data[M];
    int line;
    int i = 0;

    while(myfile >> line){
      data[i++] = line;
    }
    myfile.close();
    return sum(M,data);
}

void execute(){
  //in pipe thread
  //for computer process to execute readyqueue commend
  if(readyqueue.empty()){//may need sync_queue
    cout<<"ready queue empty here in execute"<<endl;
    continue;
  }
  while(!readyqueue.empty()){
    result_data result;
    readyqueue_data row = readyqueue.front();
    readyqueue.pop();
    int sum = sum_file(row.file_name);
    result.client_id = row.client_id;
    result.client_sd = row.client_sd;
    result.file_name = row.file_name;
    result.sum = sum;
    summning_queue.push(result);
  }

}

void parse_admin_commend(char commend){

    char c[5];
    int n;
    if(strncmp(&commend,"T",1) == 0){
      terminate();
    }
    if(strncmp(&commend,"x",1) == 0){
      execute();
    }/*else{
    cout<<"Enter 'T' for Terminate the system"<<endl;
    cout<<"enter 'x' for Execute requests from the ready queue" << endl;
    cout<<"please enter commend :"<<endl;
    n = 0;
    char ch;
    while ((ch = getchar()) != '\n'){
      c[n++] = ch;
    }
    parse_admin_commend(c[0]);
  }*/
}

void *socket_thread_func(void *args){

  cout<<"socket thread ready"<<endl;
  struct socket_param *param = (struct socket_param*)args;
  struct sockaddr_in server_address,client_address; // holds soclet reference elements
  int create_socket, new_socket, port_no, client_len,max_sd,sd,activity;
  /*
  int socket (int family, int type, int protocol);
  */

  create_socket = socket(AF_INET, SOCK_STREAM, 0);

  if(create_socket <0){
    error("fail openting socket");
  }
  /*
  bzero(void *s, int nbyte)
  set socket all struct with null
  */
  bzero((char *)&server_address,sizeof(server_address));
  port_no = param->port_num;
  cout<<"server port: "<<to_string(port_no)<<endl;

  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(port_no);

  /*
  use bind() to bind server address
  */
  if (bind(create_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
      error("binging fail");
   }
   //listen to client
   listen(create_socket,128);
   client_len = sizeof(client_address);
   cout<<"server socket start listening"<<endl;


   bool running = true;

   while(running){

     FD_ZERO(&socket_fdset); //init socket fd set
     FD_SET(create_socket, &socket_fdset);

     max_sd = create_socket;
     int k = 0;
     //add child sockets to set
     for ( int i ; i < client_set.size(); ++i) {
       //socket descriptor
       sd = client_set[i];
       //if valid socket descriptor then add to read list
       if(sd > 0) {
           FD_SET( sd , &socket_fdset);
       }else{
         close(sd);
         client_set.erase(client_set.begin()+i);
       }
       //highest file descriptor number, need it for the select function
       if(sd > max_sd) {
           max_sd = sd;
       }
     }

     activity = select(max_sd + 1 , &socket_fdset , NULL , NULL , NULL);

     if ((activity < 0) && (errno!=EINTR)){
       error("selection error");
     }
     //new connection
     if (FD_ISSET(create_socket, &socket_fdset)){
       new_socket = accept(create_socket,(struct sockaddr*)&client_address,(socklen_t *) &client_len);
       if (new_socket<0){
         error("fail to accept");
       }
       cout<<"connect to new client"<<endl;
       client_set.push_back(new_socket); //try vector here
       //try store client addr
     }
     /*
     for (size_t j = 0;j<client_set.size();j++){
       sd = client_set[j];
       if (FD_ISSET( sd , &socket_fdset)){
         //Check if it was for closing , and also read the incoming message
         computer_process(sd);
       }
      }
      */
     }
  return 0;
}

void *client_thread(void*){

  //FD_ZERO(&socket_fdset); //init socket fd set
  //FD_SET(create_socket, &socket_fdset);
  char buf[50];
  int sd;

  while(true){

      for ( int i ; i < client_set.size(); ++i) {
        //socket descriptor
        sd = client_set[i];
        cout<<"client SD :"<<sd<<endl;
        if (!FD_ISSET( sd , &socket_fdset)){
          //Check if it was for closing , and also read the incoming message
          client_set.erase(client_set.begin()+i);
          continue;
        }
        computer_process(sd);
      }
  }
return 0;
}


 void *pipe_thread_func(void *args){

   cout<<"pip thread ready"<<endl;
   struct pipe_param *param = (struct pipe_param*)args;
   int c_read = param->c_read;
   int c_write = param->c_write;
   char pipe_buf[10];
   int result;

   write(c_write,"connected",10);
   while(1){
      bzero(pipe_buf,10);
      result = read(c_read, pipe_buf,10);
      //cout<<pipe_buf[0]<<endl;
      parse_admin_commend(pipe_buf[0]);
       if (result <0){
           perror ("write in if");
           exit (2);
       }
   }
   return 0;
 }


int main(int argc, char *argv[]){

    if(argc < 2){
      //print here error
      fprintf(stderr,"usage %s port number, sleep time while doing sum\n", argv[0]);
      exit(0);
    }

     //fork param
     pid_t pid;
     int i;
     string ID;

     pipe (fds1);//pipe before fork
     pipe (fds2);

     int c_read = fds1[0];
     int c_write = fds2[1];
     int p_read = fds2[0];
     int p_write = fds1[1];

     sleep_time = argv[1];

     signal(SIGINT, signal_handler);//signal function before fork()

     pid = fork(); // create computer

     if(pid < 0){

       error("fail to fork process");

     }else if(pid > 0){
       //admin
       string line;
       ID = "admin";
       cout<<ID<<endl;
       int n,result;
       char pipe_buf[50];
       close (c_read); close (c_write);

       read(fds2[0],pipe_buf,20);
       cout<<"in admin :"<<pipe_buf<<endl;
       while(1){
         cout<<"Please enter commend: "<<endl;;
         n = 0;
         char c;
         while (( c = getchar()) != '\n'){
           pipe_buf[n++] = c;
         }
         //cout<<"pipe buff"<<pipe_buf[0]<<endl;
         if(strcmp(&pipe_buf[0], "Q")){
           kill(pid, SIGCHLD);//no SIGRTMIN in c++, use SIGCHLD instead
         }
         if(strcmp(&pipe_buf[0], "x")){
           result = write (p_write,pipe_buf,sizeof(pipe_buf));
           if (result <0){
                perror ("read x");
                exit (2);
            }
         }
         if(strcmp(&pipe_buf[0], "T")){
           result = write (p_write,pipe_buf,sizeof(pipe_buf));
           if (result < 0){
                perror ("read T");
                exit (2);
            }
            break;
         }else{
           cout<<"Please enter commend: "<<endl;;
           n = 0;
           char c;
           while (( c = getchar()) != '\n'){
             pipe_buf[n++] = c;
           }
         }
         /*
         result = write (p_write,pipe_buf,sizeof(pipe_buf));
         if (result <0){
              perror ("read");
              exit (2);
          }*/
          bzero(pipe_buf,20);
          read(p_read,pipe_buf,sizeof(pipe_buf));
      cout<<"in admin in while:"<<pipe_buf<<endl;
      }
      exit(0);
     }else{
       //computer proess
       ID = "computer";
       cout<<ID<<endl;

       socket_param s_param;
       pipe_param p_param;

       s_param.port_num = stoi(argv[1]);

       close (p_read); close (p_write); //child process
       p_param.c_write = c_write;
       p_param.c_read = c_read;

       //create thread here
       pthread_t tid1[2],tid2[2],tid3[2];
       int s_th,p_th,c_th;

      s_th = pthread_create(&tid2[1],NULL,socket_thread_func,(void*)&s_param);
      if(s_th<0){
        error("fail to create socket thread");
      }

      p_th = pthread_create(&tid1[1],NULL,pipe_thread_func,(void*)&p_param);
      if(p_th<0){
        error("fail to create pipe thread");
      }

      p_th = pthread_create(&tid3[1],NULL,client_thread,NULL);
      if(p_th<0){
        error("fail to create pipe thread");
      }

      pthread_join(tid2[1],NULL);
      pthread_join(tid1[1],NULL);
      pthread_join(tid3[1],NULL);
      //wait(NULL);
      exit(0);
     }
 }
