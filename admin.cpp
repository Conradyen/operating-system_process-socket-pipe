#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fstream>
#include <sys/time.h>
#include <pthread.h>
#include <queue>
#include <sstream>
#include <vector>
#include <signal.h>

#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
using namespace std;

int fds1[2], fds2[2];

struct clientset{
  // to be delete
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
};

void error(const char *msg){
  perror(msg);
  exit(1);
}

queue<result_data> summning_queue; //try global variable
queue<readyqueue_data> readyqueue;//make ready queue golbal
int client_set[1024]; // holds client socket sd
useconds_t sleep_time;//set sleep time in sum_file
int max_sd;//current max socket sd number
fd_set socket_fdset;//set of socket descriptors

void signal_handler(int sig_num){
  /**
   * when receive Q from admin Dump ready queue content to admin throught pipe
   */
  char write_admin_buf[1024];
  queue<readyqueue_data> tmp_queue = readyqueue;
  readyqueue_data front = tmp_queue.front();
  if(!tmp_queue.empty()){
    strcpy(write_admin_buf,front.client_id.c_str());
    strcat(write_admin_buf," ,");
    strcat(write_admin_buf,front.file_name.c_str());
    strcat(write_admin_buf," ,");
    strcat(write_admin_buf,front.client_sd.c_str());
    strcat(write_admin_buf," ,");
    strcat(write_admin_buf,front.client_port_num.c_str());
    strcat(write_admin_buf,"\n");
    tmp_queue.pop();
  }else{
    write (fds2[1],"Empty Ready Queue!",sizeof(write_admin_buf));
  }
  while(!tmp_queue.empty()){
    front = tmp_queue.front();
    tmp_queue.pop();
    strcat(write_admin_buf,front.client_id.c_str());
    strcat(write_admin_buf," ,");
    strcat(write_admin_buf,front.file_name.c_str());
    strcat(write_admin_buf," ,");
    strcat(write_admin_buf,front.client_sd.c_str());
    strcat(write_admin_buf," ,");
    strcat(write_admin_buf,front.client_port_num.c_str());
    strcat(write_admin_buf,"\n");
  }
  write (fds2[1],write_admin_buf,sizeof(write_admin_buf));
  //reset signal
  signal(SIGTERM, signal_handler);
}

readyqueue_data create_queue_in(string client_id,string file_name,string client_sd,string client_port_num){
  /**
   * coustruct data to push in queue
   * @param file_name file name from client
   */
  readyqueue_data row;
  row.client_id = client_id;
  row.file_name = file_name;
  row.client_sd = client_sd;
  row.client_port_num = client_port_num;
  return row;
}

readyqueue_data parse_msg(char* buffer,int socket,int client_port_num){
  /**
   * parse msg to add to ready queue
   * @param buffer received message form clients
   */
  string line = string(buffer),word;
  stringstream ss(line);
  vector<string> split;
  while(getline(ss, word,' ')) {
    split.push_back(word);
  }
  return create_queue_in(split[0],split[1],to_string(socket),to_string(client_port_num));
}

void send_result(result_data data){
  /**
   * Whern received x commend form admin process send sum reault to client after compute.
   * @param buffer message to send
   */

  char buffer[50];
  string client_id;
  string client_sd;
  string file_name;
  int sum;
  bzero(buffer,50);

  client_id = data.client_id;
  client_sd = data.client_sd;
  file_name = data.file_name;
  sum = data.sum;
  strcpy(buffer,client_id.c_str());
  strcat(buffer," ,");
  strcat(buffer,file_name.c_str());
  strcat(buffer," ,");
  strcat(buffer,to_string(sum).c_str());
  if (FD_ISSET(stoi(client_sd) , &socket_fdset)){
    write(stoi(client_sd),buffer,sizeof(buffer));
  }else{
    cout<<"client :"<<client_id<<" closed"<<endl;
  }
}

void computer_process(int socket,int i){
  /**
   * read file name forom clients and store to readyqueue. Only call in
   * computer socket thread.
   * @param buffer buffer for read message.
   */
  char buffer[256];
  int read_msg;
  struct sockaddr_in client_address;
  int client_port,addr_len;
  bzero(buffer,256);

  getpeername(socket , (struct sockaddr*)&client_address ,(socklen_t*)&addr_len);
  client_port = ntohs(client_address.sin_port);

  read_msg = read(socket,buffer,256);
  if(read_msg<0){
    //error
    error("fail to read from client");
  }
  else if(strncmp(buffer,"nullfile",7) == 0){
    //check client status
    close(socket);
    client_set[i] = 0;
  }else{
    //if not nullfile push file name into queue
    readyqueue_data row = parse_msg(buffer,socket,client_port);
    readyqueue.push(row);
    if(readyqueue.empty()){
      error("fail to put in queue, in soclet thread");
    }
    if(read_msg<0){
      error("fail to response to client");
    }
  }
}

int sum(int M,int file[]){
  /**
   * sum number
   */
  int sum = 0;
  for(int i = 0;i < M;i++){
    sum += file[i];
    usleep(sleep_time*1000);
  }
  return sum;
}

int sum_file(string file_name){
    /**
     * open file and sum numbers
     * @param file_name file to sum.
     */
    ifstream myfile;
    //filestruct data_out;
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
  /**
   * When admin send x commend do sum for all file in readyqueue.
   * Only call in in pipe thread.
   */

  if(readyqueue.empty()){
    cout<<"ready queue empty!"<<endl;
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
    //cout<<"here before send"<<endl;
    send_result(result);
  }

}

void parse_admin_commend(char commend){
  /**
   * parse commend from admin process. Call in pipe thread.
   */
    char c[5];
    int n;
    if(strncmp(&commend,"T",1) == 0){
      terminate();
    }
    if(strncmp(&commend,"x",1) == 0){
      execute();
    }
}

void *socket_thread_func(void *args){

  struct socket_param *param = (struct socket_param*)args;
  struct sockaddr_in server_address,client_address; // holds soclet reference elements
  int create_socket, new_socket, port_no, client_len,sd,activity;
  /*
  *int socket (int family, int type, int protocol);
  */

  create_socket = socket(AF_INET, SOCK_STREAM, 0);

  if(create_socket <0){
    error("fail openting socket");
  }
  /*
  *set socket all struct with null
  */
  bzero((char *)&server_address,sizeof(server_address));
  port_no = param->port_num;

  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(port_no);

  /*
  *use bind() to bind server address
  */
 bind(create_socket, (struct sockaddr *) &server_address, sizeof(server_address));

   //listen to client
   listen(create_socket,128);
   client_len = sizeof(client_address);
   cout<<"Computer process server socket ready."<<endl;

   while(true){

     FD_ZERO(&socket_fdset); //init socket fd set
     FD_SET(create_socket, &socket_fdset);

     max_sd = create_socket;
     int k = 0;
     //add child sockets to set
     for ( int i  = 0; i < 1024; i++) {
       //socket descriptor
       sd = client_set[i];
       //if valid socket descriptor then add to read list
       if(sd > 0) {
           FD_SET( sd , &socket_fdset);
       }
     }
     //new connection
     if (FD_ISSET(create_socket, &socket_fdset)){
       new_socket = accept(create_socket,(struct sockaddr*)&client_address,(socklen_t *) &client_len);
       if (new_socket<0){
         error("fail to accept");
       }

       for(int idx = 0; idx < 1024;idx++){
         if(client_set[idx] == 0){
           client_set[idx] = new_socket;
           break;
         }
       }
      }
     }
  return 0;
}

void *client_thread(void*){
  /**
  *interact with clients & handle disconnect
  */
  //FD_ZERO(&socket_fdset); //init socket fd set
  //FD_SET(create_socket, &socket_fdset);
  char buf[50];
  int sd;

  while(true){
      for ( int i  = 0; i < 1024;i++) {
      //socket descriptor
      sd = client_set[i];
        if (FD_ISSET( sd , &socket_fdset)){
          //Check if it was for closing , and also read the incoming message
          //if client sends nullfile it's terminate
          computer_process(sd,i);
        }
      }
  }
return 0;
}

void *pipe_thread_func(void *args){
   /**
    * computer process pipe thread function.
    * @param c_read: child pipe read
    * @param c_write: child pipe write
    */
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
      parse_admin_commend(pipe_buf[0]);//add write
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

     //set sleep_time before fork() to pass to compiter process
     sleep_time = stoi(argv[1]);

     pid = fork(); // create computer

     if(pid < 0){

       error("fail to fork process");

     }else if(pid > 0){
       //admin process
       string line;
       ID = "Admin";
       cout << ID << " , " << pid << " , " << getpid() << endl;
       int n,result;
       char pipe_buf[1024];
       close (c_read); close (c_write);

       read(fds2[0],pipe_buf,20);

       while(1){
         char dump_buf[1024];
         cout<<"Admin commend: ";
         cin>>line;
         strcpy(pipe_buf,line.c_str());
         if(strncmp(&pipe_buf[0], "Q",1) == 0){
           //send Q through signal
           kill(pid, SIGTERM);
           read(p_read,dump_buf,sizeof(dump_buf));
           cout<<dump_buf;
           bzero(dump_buf,1024);
           continue;
         }
         //x,T thronght pipe
         else if(strncmp(&pipe_buf[0], "x",1) == 0){
           //send x through pipe
           result = write (p_write,pipe_buf,sizeof(pipe_buf));
           if (result <0){
                perror ("read x");
                exit (2);
            }
            continue;
         }
         else if(strncmp(&pipe_buf[0], "T",1) == 0){
           //send T through pipe
           result = write (p_write,pipe_buf,sizeof(pipe_buf));
           if (result < 0){
                perror ("read T");
                exit (2);
            }
            break;
         }else{
           cout<<"Please enter commend: in else"<<endl;;
           n = 0;
           char c;
           while (( c = getchar()) != '\n'){
             pipe_buf[n++] = c;
           }
         }
      }
     }else{
       //computer proess
       ID = "Computer";
       cout << ID << " , " << pid << " , " << getpid() << endl;

       socket_param s_param;//socket thread
       pipe_param p_param;//pipe thead

       //set up parameters for socket thread
       s_param.port_num = stoi(argv[1]);

       //close pipe for parent process
       close (p_read); close (p_write);
       //set up parameters for pipe thread
       p_param.c_write = c_write;
       p_param.c_read = c_read;

       //create thread here
       pthread_t tid1[2],tid2[2],tid3[2];
       int s_th,p_th,c_th;

       signal(SIGTERM, signal_handler);
       //socket thread
      s_th = pthread_create(&tid2[1],NULL,socket_thread_func,(void*)&s_param);
      if(s_th<0){
        error("fail to create socket thread");
      }
      //pipe thread
      p_th = pthread_create(&tid1[1],NULL,pipe_thread_func,(void*)&p_param);
      if(p_th<0){
        error("fail to create pipe thread");
      }
      //client thread interace with client and handle disconnections
      c_th = pthread_create(&tid3[1],NULL,client_thread,NULL);
      if(c_th<0){
        error("fail to create pipe thread");
      }

      pthread_join(tid2[1],NULL);
      pthread_join(tid1[1],NULL);
      pthread_join(tid3[1],NULL);

     }
 }
