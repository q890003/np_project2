#include <iostream>
#include <string>		
#include <cstring>		//strchr()
#include <vector>
#include <algorithm>    // std::replace
#include <sstream>

#include <stdio.h>  		// fprint error
#include <stdlib.h>		//setenv

#include <unistd.h>		// wait(),exec(), pipe(), fork()
#include <sys/types.h> //								fork() 
#include <sys/wait.h>  //wait()
#include <sys/stat.h>  // fifo

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <fcntl.h>		//open()
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

using namespace std;
#define CLIENT_LIMIT 31
#define CLIENT_NAME_SIZE 20
#define UPIPE_NAME_SIZE 20
#define SHARE_MSG_SIZE 1024
#define INPUT_BUFFER_SIZE 15000
#define CLIST_KEY 123
#define SHARE_MSG_KEY 124
#define UPIPE_KEY 125
#define PORT_SIZE	6
#define MEMBER_JOIN 0
#define MEMBER_LEAVE 1
#define NAME_CHANGE 2
#define MEMBER_YELL 3
#define USER_PIPE_SENDER 4
#define USER_PIPE_RECIEVER 5

class Pipe_class{
	public: 
	int get_read(){
		return pfd[0];
	}
	int get_write(){
		return pfd[1];
	}
	void set_read(int read){
		pfd[0] = read;
	}
	void set_write(int write){
		pfd[1] = write;
	}
	int get_count(){
		return numPipe_count;
	}
	void set_numPipe_count(int i){
		numPipe_count	= i;
	}
	void count_decrease(){
		numPipe_count--;
	}
	void creat_pipe(){
		pipe(pfd);
	}
	void close_pipe(){
		close(pfd[0]);
		close(pfd[1]);
	}
		int pfd[2];
		int numPipe_count;
};
class Client_state{
	public:
		void init(){
			pid = -1;
			memset( name, '\0', CLIENT_NAME_SIZE);
			strcpy(name,"(no name)");
			memset( IP, 0, INET_ADDRSTRLEN);
			port = -1;
			broadcast_flag = false;
		}
		void change_name(const char* newname){
			memset( name, '\0', CLIENT_NAME_SIZE );
			strncpy(name, newname, strlen(newname) );
		}
		pid_t pid;
		char name[CLIENT_NAME_SIZE] = {0};
		char IP[INET_ADDRSTRLEN] = {0};
		unsigned short int port;
		bool broadcast_flag = false;
};
class Sh_mem{
	public:
		//size, key, shmid stored in class. 
		void creat_shmid(key_t key_temp, int size_temp){
			key = key_temp;
			size = size_temp;
			shmid = shmget( key, size, 0644 | IPC_CREAT);
			if (shmid == -1){
				fprintf (stderr, "shmget failed\n");
				exit (1);
			}
		}
		void getVoid_ptr(void* ptr){
			attach_ptr = ptr;
		}
		void write_msg(const char* inut_msg){
			memset((char*)attach_ptr,0,sizeof(SHARE_MSG_SIZE));
			strcpy((char*)attach_ptr, inut_msg);
		}
		void CList_init(){
			Client_state* Client_List = (Client_state*)attach_ptr;
			for(int i = 0; i < CLIENT_LIMIT; i++)	  //start from 0, in case of error.
				Client_List[i].init();
		}
		int register_client(int input_pid, char input_ip[INET_ADDRSTRLEN] , unsigned short int input_port){
			Client_state* Client_List = (Client_state*)attach_ptr;
			for(int i = 1; i < CLIENT_LIMIT; i++){
				if(Client_List[i].pid == -1 ){
					Client_List[i].pid = input_pid;
					strcpy(Client_List[i].IP, input_ip);
					Client_List[i].port = input_port;
					break;
				}
			}
			return -1;
		}
		int broadcast_read_check(Client_state *Client_List){
			for(int i = 1; i < CLIENT_LIMIT; i++){
				if(Client_List[i].pid != -1){
					while(Client_List[i].broadcast_flag == true){	//true means there is broadcast msg.
						//do nothing. wait till it accept brocast msg.
					}
				}
			}

		}
		void* attach_ptr;
		key_t key;
		int shmid;
		int size;
};
class Upipe_receive{
	public:
		int to[CLIENT_LIMIT];
		char Upipe_name_to_receiver[CLIENT_LIMIT][UPIPE_NAME_SIZE];
};
class Upipe_send{
	public:
		Upipe_receive sender[CLIENT_LIMIT];
};
vector<Pipe_class> pipe_vector;

void signalhandler(int);
void convert_argv_to_consntchar(const char, vector<string> );
bool isSpecial_cmd(stringstream&);
bool isChat_cmd(stringstream &);
void parse_cmd(stringstream &);
int get_client_id(int );
void get_cmd(char*);
void broadcast( int , char*);
bool isUserExist(int);
vector<string> retrieve_argv(stringstream&);
////////////////////////////////////////////////////////////////
///////////////////////Global Variable//////////////////////////
////////////////////////////////////////////////////////////////
Sh_mem CList_shm, MSG_shm, Upipe_shm;
Client_state *Client_List;
Upipe_send* Upipe;
char *shm_msg;
int read_record[31];
int Upipe_receieverID = 0;
int Upipe_senderID = 0;
int UserID = -1;
bool shell_exit = false;
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

int socket_fd =0;
int client_fd = 0;

int main(int argc, char* argv[]){
	////////////////////////////////////////////////////////////////
	///////////////////////connection///////////////////////////////
	////////////////////////////////////////////////////////////////
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1){
        cout <<"Fail to create a socket." << endl;
    }
	int opt = 1;
	setsockopt(socket_fd, SOL_SOCKET,SO_REUSEADDR, &opt, sizeof(opt));
	setsockopt(socket_fd, SOL_SOCKET,SO_REUSEPORT, &opt, sizeof(opt));
	struct sockaddr_in serverInfo,clientInfo;
	socklen_t  addrlen = sizeof(clientInfo);
	bzero(&serverInfo,sizeof(serverInfo));  //initiialize server info.
	bzero(&serverInfo,sizeof(clientInfo));  //initiialize client info.
	serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(atoi(argv[1]));
	bind(socket_fd, (struct sockaddr *)&serverInfo, sizeof(serverInfo));
	listen(socket_fd, 30);


	////////////////////////////////////////////////////////////////
	///////////////share mem data initialization////////////////////
	////////////////////////////////////////////////////////////////
	void* temp;
	//msg 
	MSG_shm.creat_shmid(SHARE_MSG_KEY, SHARE_MSG_SIZE); 
	temp = shmat(MSG_shm.shmid, NULL, 0);		
	if (temp == (void *)-1){
		fprintf (stderr, "shmat failed\n");
		exit (1);
	}
	MSG_shm.getVoid_ptr(temp);
	shm_msg	= (char*)MSG_shm.attach_ptr;
	memset(shm_msg,0,sizeof(SHARE_MSG_SIZE));


	//31 Clients
	CList_shm.creat_shmid(CLIST_KEY, CLIENT_LIMIT*sizeof(Client_state));
	temp = shmat(CList_shm.shmid, NULL, 0);		
	if (temp == (void *)-1){
		fprintf (stderr, "shmat failed\n");
		exit (1);
	}
	CList_shm.getVoid_ptr(temp);
	CList_shm.CList_init();
	Client_List = (Client_state *)CList_shm.attach_ptr;
	

	//31*31 Upipe matrix. skip [0][x], [x][0],
	Upipe_shm.creat_shmid(UPIPE_KEY, sizeof(Upipe_send));
	temp = shmat(Upipe_shm.shmid, NULL, 0);		
	if (temp == (void *)-1){
			fprintf (stderr, "shmat failed\n");
			exit (1);
	}
	Upipe_shm.getVoid_ptr(temp);
	Upipe = (Upipe_send*)Upipe_shm.attach_ptr;
	for(int i = 0; i < CLIENT_LIMIT; i++)
		for(int j = 0; j < CLIENT_LIMIT; j++){
			Upipe->sender[i].to[j] = false;
			memset(Upipe->sender[i].Upipe_name_to_receiver[j],0,UPIPE_NAME_SIZE);
			sprintf(Upipe->sender[i].Upipe_name_to_receiver[j],"user_pipe/%dto%d", i, j);
		}
			
	signal(SIGINT, signalhandler);
	signal(SIGUSR1, signalhandler);		//for broadcast, and tell.
	signal(SIGUSR2, signalhandler);		//for Upipe.
	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	/////////////////basic env for each shell///////////////////////
	////////////////////////////////////////////////////////////////
	setenv("PATH", "bin:.", 1); 
	string cmd;
	stringstream sscmd;
	memset(read_record, -1, sizeof(read_record));
	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////

	//server listernig loop.
	while(true){

		client_fd = accept(socket_fd, (struct sockaddr*) &clientInfo, &addrlen);
		pid_t replica_pid = fork();
		if(replica_pid == -1){
			cout << "fork failed" << endl;

		}else if(replica_pid > 0){
			char IP[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(clientInfo.sin_addr), IP, INET_ADDRSTRLEN);
			int port =  htons(clientInfo.sin_port);
			CList_shm.register_client(replica_pid, IP, port);
			close(client_fd);

		
		}else if(replica_pid == 0){ ////////////client/////////

			//////////client initialization////////////////
			////////////////////////////////////////////////
			dup2(client_fd, STDIN_FILENO);
			dup2(client_fd, STDOUT_FILENO);
			dup2(client_fd, STDERR_FILENO);
			close(client_fd);
			UserID = get_client_id(getpid());
			cout <<"****************************************\n** Welcome to the information server. **\n****************************************" << endl;
			broadcast(MEMBER_JOIN, NULL);
			string cmd;
			char input_buffer[INPUT_BUFFER_SIZE]= {0};
			char temp[INPUT_BUFFER_SIZE];
			//////////end client initialization////////////////
			//////////////////////////////////////////////////

			
			while(true){
				cout << "% " << flush;
				///////////////////////////////////////////
				//////////input cmd processing/////////////
				///////////////////////////////////////////
				memset(input_buffer,0,strlen(input_buffer));
				read(STDIN_FILENO, input_buffer, sizeof(input_buffer) );
				int k =0;
				memset( temp, '\0', sizeof(temp) );
				strcpy(temp, input_buffer); 
				for(int i = 0; i<= strlen(temp); i++ ){
					if(temp[i] == '\r' || temp[i] == '\n')
						continue;

					input_buffer[k] = temp[i];
					k++;
				}
				cmd = input_buffer;
				sscmd.str("");
				sscmd.clear();
				sscmd.str(cmd);
				///////////////////////////////////////////
				///////////////////////////////////////////


				if( !isSpecial_cmd(sscmd)){
					if(shell_exit == true){
						break;		//leave while loop
					}else if(isChat_cmd(sscmd) ){
						//no op
					}else{
						parse_cmd(sscmd);
					}
				}
			}

			broadcast(MEMBER_LEAVE, NULL);
			Client_List[UserID].init();
			//release unread Upipe
			for(int i = 1 ; i < CLIENT_LIMIT; i++){
				if(Upipe->sender[UserID].to[i] == true){
					Upipe->sender[UserID].to[i] = false;
					unlink(Upipe->sender[UserID].Upipe_name_to_receiver[i]);
					//need a mechenism to tell receiver to close readfd.
				}
				if(Upipe->sender[i].to[UserID] == true){	
					close(read_record[i]);
					Upipe->sender[i].to[UserID] = false;
					unlink(Upipe->sender[i].Upipe_name_to_receiver[UserID]);
					//  read_record[i] = -1;  //no need to reset read. 
				}
			}
			exit(-1);
		}
		//////////////////////////
	}


}
bool isUserExist(int lookingID){
	if(Client_List[lookingID].pid != -1 ){
		return true;
	}
	return false;
}
bool isChat_cmd(stringstream &sscmd){
	string cmdline = sscmd.str();
	stringstream chat_cmd;
	chat_cmd << cmdline;
	vector<string> argv_table = retrieve_argv(chat_cmd);   // parse out cmd before sign |!>
	if (argv_table.empty() ){
		return true;
	}
	if(argv_table.at(0) == "who" ){
		cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
		for(int i = 1; i < CLIENT_LIMIT; i++){
			if(Client_List[i].pid != -1 ){
				if(Client_List[i].pid == getpid() ){
					cout << i << "\t" << Client_List[i].name << "\t" << Client_List[i].IP<<":" << Client_List[i].port<< "\t" <<  "<-me" << endl;
				}else{
					cout << i << "\t" << Client_List[i].name << "\t" << Client_List[i].IP<<":" << Client_List[i].port<<endl;
				}
			}
		}
		return true;
	}
	if(argv_table.at(0) == "name" ){
		bool isName_exist = false;
		for(int i = 1; i < CLIENT_LIMIT; i++){
			if(strcmp(argv_table.at(1).c_str(), Client_List[i].name) == 0 ){
				isName_exist = true;
			}
		}
		if(isName_exist == true){
			cout << "*** User '" << argv_table.at(1) <<"' already exists. ***"<< endl;
		}else{	
			memset(Client_List[ UserID ].name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			Client_List[UserID ].change_name(argv_table.at(1).c_str() ) ;
			broadcast(NAME_CHANGE, NULL);
		}
		return true;
	}
	if(argv_table.at(0) == "yell" ){
		char message[1024] = {0};
		sprintf(message," %s",cmdline.substr(5,cmdline.length()).c_str()),
		broadcast(MEMBER_YELL, message);
		return true;
	}				
	if(argv_table.at(0) == "tell" ){
		if(!isUserExist( atoi(argv_table.at(1).c_str()) )  ){
			cout << "*** Error: user #" << argv_table.at(1).c_str() << " does not exist yet. ***" << endl;
			return true;
		}
		char message[1024];
		sprintf(message,"*** %s told you ***: %s",Client_List[ UserID ].name, cmdline.substr(7,cmdline.length()).c_str());
		
		MSG_shm.write_msg(message);	
		kill(Client_List[atoi(argv_table.at(1).c_str() )].pid, SIGUSR1);
		return true;
	}	
	return false;;
}

void broadcast( int action, char* msg){
	char broadcast_content[SHARE_MSG_SIZE];
	memset(broadcast_content,0,strlen(broadcast_content));
	switch(action){
		case MEMBER_JOIN:
			sprintf(broadcast_content,"*** User '%s' entered from %s:%d. ***",Client_List[UserID].name, 
																			  Client_List[UserID].IP,
																			  Client_List[UserID].port);
		break;
		case MEMBER_LEAVE:
			sprintf(broadcast_content,"*** User '%s' left. ***", Client_List[UserID].name);
		break;
		case NAME_CHANGE:
			sprintf(broadcast_content,"*** User from %s:%d is named '%s'. ***", Client_List[UserID].IP,
																			   Client_List[UserID].port,
																			   Client_List[UserID].name);
		break;
		case MEMBER_YELL:
			sprintf(broadcast_content,"*** %s yelled ***: %s", Client_List[UserID].name, msg);
		break;
		case USER_PIPE_SENDER:
			sprintf(broadcast_content,"***  %s (#%d) just piped '%s' to %s (#%d) ***", Client_List[UserID].name, UserID,
																				  		msg, Client_List[Upipe_receieverID].name, Upipe_receieverID);
		break;
		case USER_PIPE_RECIEVER:
			sprintf(broadcast_content,"***  %s (#%d) just received from %s (#%d) by '%s' ***", Client_List[UserID].name, UserID,
																				  			  Client_List[Upipe_senderID].name, Upipe_senderID, msg);
		break;
	}
	CList_shm.broadcast_read_check(Client_List);
	MSG_shm.write_msg(broadcast_content);
	for(int j= 1; j < CLIENT_LIMIT; j++){			
		if(Client_List[j].pid  !=  -1 ){  
			Client_List[j].broadcast_flag = true;
			kill(Client_List[j].pid, SIGUSR1);
		}
	}
}


int get_client_id(int pid){
	for(int i = 1; i < CLIENT_LIMIT; i++)
		if(((Client_state *)Client_List)[i].pid == pid )
			return i;

	return -1;
}


void signalhandler(int signo){
	switch(signo){
		case SIGCHLD:
			int status;
			while (waitpid(-1, &status, WNOHANG) > 0);
		break;
		case SIGINT:
			shmdt(CList_shm.attach_ptr);		//detach CList_shm
			shmctl(CList_shm.shmid,IPC_RMID,0);	//close  CList_shm
			shmdt(MSG_shm.attach_ptr);			//detach MSG_shm
			shmctl(MSG_shm.shmid,IPC_RMID,0);	//close  MSG_shm
			shmdt(Upipe_shm.attach_ptr);		//detach Upipe_shm
			shmctl(Upipe_shm.shmid,IPC_RMID,0);	//close  Upipe_shm
			close(socket_fd);
			exit(1);
		break;
		case SIGUSR1:	//get broadcast and tell msg.
			cout << shm_msg << endl;
			Client_List[UserID].broadcast_flag = false;
		break;
		case SIGUSR2:	//get Upipe sig.
			for(int i = 1; i < CLIENT_LIMIT; i++){
				if(read_record[i] == -1 &&  Upipe->sender[i].to[UserID] == true ){
					read_record[i] = open(Upipe->sender[i].Upipe_name_to_receiver[UserID],O_RDONLY);
				}
			}
		break;
	}
}
void convert_argv_to_consntchar(const char *argv[], vector<string> &argv_table) {
	for (int i=0; i<argv_table.size(); i++) {
		argv[i] = argv_table[i].c_str();
	}
	argv[argv_table.size()] = NULL;
}

void parse_cmd(stringstream &sscmd){
	bool getUpipe_success;
	bool create_user_pipe_flag;
	bool Upipe_err_flag; 
	bool pipe_flag;	
	bool shockMarckflag;
	bool file_flag;
	bool target_flag;
	Pipe_class current_pipe_record;
	Pipe_class pipe_reached_target;

	while( !sscmd.eof()){      			//check sstream of cmdline is not eof.
		//unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
		int newProcessIn = STDIN_FILENO;   //shell process's fd 0,1,2 are original one. never changed.
 		int newProcessOut = STDOUT_FILENO;
		int newProcessErr = STDERR_FILENO;
		getUpipe_success = false;
		create_user_pipe_flag = false;
		Upipe_err_flag = false;
		pipe_flag = false;
		shockMarckflag = false;
		file_flag	= false;
		target_flag = false;
		

		string teststring = sscmd.str();
		vector<string> argv_table = retrieve_argv(sscmd);   // parse out cmd before sign |!><
		if (argv_table.empty()   ){
			break;
		}

		string sign_number;	
		sscmd >> sign_number;
		if( (sscmd>>ws)  &&    strchr("<", sscmd.peek() )  ){	// chekc if < is after >, if so, push '<ID' into argv.
			string peek_cmd;
			sscmd >> peek_cmd;
			argv_table.push_back(peek_cmd);
		}
		//check if there is <, recieving userpipe
		if(argv_table.size() >=2){
			vector<string>::iterator peek_recieve = argv_table.end()-1;	
			if((*peek_recieve)[0] == '<' && isdigit( (*peek_recieve)[1]) ){
				
				string sign_number = (*peek_recieve).substr(1); //skip the first charactor.	
				replace(sign_number.begin(), sign_number.end(), '+', ' ');
				stringstream numb(sign_number);
				int pipenumber = 0;
				int temp;
				while (numb >> temp){
					pipenumber += temp;
				}
				if( read_record[pipenumber] != -1 && Upipe->sender[pipenumber].to[UserID] == false ){
					read_record[pipenumber] = -1;  //sender left. and client needs to update read_record.
				}
				if( read_record[pipenumber] != -1 && Upipe->sender[pipenumber].to[UserID] == true ){
					getUpipe_success = true;
					argv_table.pop_back();
					Upipe_senderID = pipenumber;		//for Upipe broadcast and close Upipe
					newProcessIn = read_record[pipenumber];


					//reset upipe's shareMem
					read_record[pipenumber] = -1;
				}

				//check if Upipe accept success. otherwise user(ID)/Upipe not exist.
				if(isUserExist(pipenumber) == false){	 //if not exist, return 0;
					Upipe_err_flag = true;
					argv_table.pop_back();
					cout <<"*** Error: user #" << pipenumber <<" does not exist yet. ***" << endl;
					newProcessIn = open("/dev/null", O_RDWR);	
				}else if(getUpipe_success == false){
					Upipe_err_flag = true;
					argv_table.pop_back();
					cout <<"*** Error: the pipe #" << pipenumber << "->#" << UserID << " does not exist yet. ***" << endl;
					newProcessIn = open("/dev/null", O_RDWR);
				
				}else{		
					char user_pipe_msg[1024] = {0};
					string tmp = sscmd.str();
					strcpy(user_pipe_msg, tmp.c_str());
					broadcast(USER_PIPE_RECIEVER, user_pipe_msg);
				}
			}
		}
		
		if(sign_number[0] == '|'){
				pipe_flag = true;
				if( isdigit( sign_number[1]) ){			//don't pop sign, in case of '|' at the end of cmd.
					sign_number = sign_number.substr(1); //skip the first charactor.	
					replace(sign_number.begin(), sign_number.end(), '+', ' ');
					stringstream numb(sign_number);
					int test = 0;
					int pipenumber = 0;
					while (numb >> test){
						pipenumber += test;
					}
					current_pipe_record.set_numPipe_count(pipenumber);
				} else {
					current_pipe_record.set_numPipe_count(1);
				}
		}
		if(sign_number[0] =='!'){
				//pratically the same as mentioned ahead.
				pipe_flag = true;
				shockMarckflag = true;
				if( isdigit( sign_number[1]) ){			//don't pop sign, in case of '|' at the end of cmd.
					sign_number = sign_number.substr(1);				//skip the first charactor.		
					current_pipe_record.set_numPipe_count(atoi(sign_number.c_str()));
				} else {
					current_pipe_record.set_numPipe_count(1);
				}
		}
		
		if(sign_number[0] =='>'){
			if( isdigit( sign_number[1]) ){
				sign_number = sign_number.substr(1); //skip the first charactor.	
				replace(sign_number.begin(), sign_number.end(), '+', ' ');
				stringstream numb(sign_number);
				int test;
				int pipenumber = 0;
				while (numb >> test){
					pipenumber += test;
				}  
				if(isUserExist(pipenumber) == false){
					Upipe_err_flag = true;
					cout << "*** Error: user #" << pipenumber << " does not exist yet. ***" << endl;
					newProcessIn = open("/dev/null", O_RDWR);
					newProcessOut = newProcessIn;
				}
				//check if user_pipe exist.
				if( Upipe->sender[UserID].to[pipenumber] == true ){
					Upipe_err_flag = true;
					cout << "*** Error: the pipe #"<< UserID<<"->#"<< pipenumber <<" already exists. ***" << endl;
					newProcessIn = open("/dev/null", O_RDWR);
					newProcessOut = newProcessIn;
				}
				//user pipe is avaliable to create.
				if(Upipe_err_flag == false){
					create_user_pipe_flag = true;
					Upipe->sender[UserID].to[pipenumber] = true;
					Upipe_receieverID = pipenumber;		//for Upipe broadcast.

					int a = mkfifo(Upipe->sender[UserID].Upipe_name_to_receiver[pipenumber], 0644);		
					kill(Client_List[pipenumber].pid, SIGUSR2);		
					newProcessOut = open(Upipe->sender[UserID].Upipe_name_to_receiver[pipenumber], O_WRONLY);
					
					//broadcast
					char user_pipe_msg[1024] ={0};
					string tmp = sscmd.str();
					strcpy(user_pipe_msg, tmp.c_str());
					broadcast(USER_PIPE_SENDER, user_pipe_msg);
				}
			}else{
				file_flag = true;
				string filename;
				sscmd >> filename;
				newProcessOut = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC , 00777);
			}
		}
		
		//if upcomming child process is target or not.
		int target_flag = false;
		int pop_out_index = -1;				//for releasig from pipe_vector
		for(int i = 0; i< pipe_vector.size(); i++){				//there would be only on input in a instruction. won't be ls | cat <0
			if((pipe_vector[i].get_count() == 0) ){
				target_flag = true;
				pop_out_index = i;
				newProcessIn = pipe_vector[i].get_read();
				pipe_reached_target = pipe_vector[i];
				close(pipe_vector[i].get_write() );			//write to pipe which arrives target will be useless. it'll be closed in both child and parent process.
			}
			//count can not decrease here. itll influence following Pioneer part.
		}
		
		//check if upcomming process_output has the same target with any current_pipe_record's.
		int isPioneer = true;
		if(pipe_flag == true){
			for(int i = 0; i< pipe_vector.size(); i++){
				if((current_pipe_record.get_count() == pipe_vector[i].get_count() )  ){		
					newProcessOut = pipe_vector[i].get_write();
					isPioneer = false;
					if(shockMarckflag == true){
						newProcessErr = pipe_vector[i].get_write();
					}
				}
			}
			if (isPioneer == true) {		//it's pioneer pipe. create it !
				current_pipe_record.creat_pipe();
				pipe_vector.push_back( current_pipe_record );
				newProcessOut = current_pipe_record.get_write();
				if(shockMarckflag == true ){
					newProcessErr = current_pipe_record.get_write();
				}
			}			
		}

		pid_t pid;
		while ( ( pid = fork() ) < 0){
			//cout << "child process creating fail" << endl;
		}
		if( pid == 0){	 											// child
			
			if(pipe_flag == true){					
				close(current_pipe_record.get_read());	//it suppose to close read of new pipe.  If other data want to use the same pipe, STD_read would be useless.
				dup2(newProcessOut, STDOUT_FILENO);  	// however, it'll effect  client taking data from client pipe. 
				if(shockMarckflag == true ){
					dup2(newProcessErr, STDERR_FILENO);		// newProcessErr = newProcessOut
				}
				close(newProcessOut);
			} else if(file_flag == true ) {
				dup2(newProcessOut, STDOUT_FILENO);	
				close(newProcessOut);
			} else if(create_user_pipe_flag == true){	//sending msg to reciever
				dup2(newProcessOut, STDOUT_FILENO);
				dup2(newProcessErr, STDERR_FILENO);
				close(newProcessOut);  //newProcessOut = newProcessErr
			}
			//if there is no fd change,  eveytime child fork starts with shell_std_in/out/err. 
			if(target_flag == true || getUpipe_success == true){
				dup2(newProcessIn, STDIN_FILENO);  
				close(newProcessIn);
			}

			if(Upipe_err_flag == true){	//overlap INput if itslef is target.
				dup2(newProcessIn, STDIN_FILENO);  
				dup2(newProcessOut, STDOUT_FILENO);  		//if it's '< error', output will overlap with other pioneer and target it's self.
				close(newProcessIn);	//in == out
			}
			
			//get argv.			
			const char *pargv[argv_table.size() + 1];
			convert_argv_to_consntchar(pargv, argv_table);
			execvp(pargv[0], (char **) pargv);
			if(execvp(pargv[0], (char **) pargv) == -1 ){
				fprintf(stderr,"Unknown command: [%s].\n",pargv[0]);
			}
			exit(-1);

			
		} else if(pid >0 ){			//parent
			usleep(10000); 
			for(int i = 0; i< pipe_vector.size(); i++){		
				pipe_vector[i].count_decrease();
			}
			if(file_flag == true ){  //close pipe_write_only 
				close(newProcessOut);	
			}
			if (getUpipe_success == true ){
				close(newProcessIn);
				read_record[Upipe_senderID] = -1;
				Upipe->sender[Upipe_senderID].to[UserID] = false;
				unlink(Upipe->sender[Upipe_senderID].Upipe_name_to_receiver[UserID]);
			}
			if(target_flag == true){
				close(pipe_reached_target.get_read());
				pipe_vector.erase(pipe_vector.begin()+pop_out_index);
			}
			if(create_user_pipe_flag == true){
				close(newProcessOut);
			}
			if(STDOUT_FILENO == newProcessOut || file_flag == true ){		//command pid want to printout to console. or wait writting to file.
				int status;															
				waitpid(pid, &status, 0);
			}

			
		}
	}
}

bool isSpecial_cmd(stringstream &sscmd){
	stringstream ss;
	string cmdline = sscmd.str();
    string parsed_word, cmd;
	
	ss << cmdline;
    ss>> parsed_word;
	if(parsed_word == "printenv"){
		ss >> parsed_word;
		char* pPath = getenv(parsed_word.c_str());
		if(pPath != NULL)
			cout << pPath << endl;;
		return true;
    }
    else if(parsed_word == "setenv"){
        ss >> cmd;
        ss >> parsed_word;
        setenv(cmd.c_str(), parsed_word.c_str(), 1) ;   
		return true;
    }else if (parsed_word == "exit"){
			shell_exit = true;
		}
	return false;
}
vector<string> retrieve_argv(stringstream &ss){

		vector<string> argv;
		string token;

		while((ss >> ws) && !strchr("|!>", ss.peek()) && (ss >> token)  )
				argv.push_back(token);
		return argv;
}