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
//#include <string.h>	//for version2 at line 135-145 strtok
//#include <stdlib.h>	//for version2 at line 135-145 strtok
using namespace std;
#define CLIENT_LIMIT 31
#define CLIENT_NAME_SIZE 20
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
		}
		void change_name(const char* newname){
			memset( name, '\0', CLIENT_NAME_SIZE );
			strncpy(name, newname, strlen(newname) );
		}
		pid_t pid;
		char name[CLIENT_NAME_SIZE] = {0};
		char IP[INET_ADDRSTRLEN] = {0};
		unsigned short int port;
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
		void* attach_ptr;
		key_t key;
		int shmid;
		int size;
};
class Upipe_receive{
	public:
		int to[CLIENT_LIMIT];
		int rfd[CLIENT_LIMIT];
		int wfd[CLIENT_LIMIT];
};
class Upipe_send{
	public:
		void Upipe_init(int leaving_clientID){
			for(int i=0; i < CLIENT_LIMIT; i++){
				
			}
		}
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
/////////Global Variable//////////////
Sh_mem CList_shm, MSG_shm, Upipe_shm;
Client_state *Client_List;
Upipe_send* Upipe;
char *shm_msg;
char upipe_name[10];
int read_record[31] = {0};
int Upipe_receiever = 0;
int Upipe_sender = 0;
bool shell_exit = false;
///////////////////////////////////////


int socket_fd =0;
int client_fd = 0;

int main(int argc, char* argv[]){
	
	///////////////////////connection////////////////////////////////

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1){
        cout <<"Fail to create a socket." << endl;
    }
	int opt = 1;
	setsockopt(socket_fd, SOL_SOCKET,SO_REUSEADDR,&opt, sizeof(opt));
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

	////////////////////share mem data initialization///////////////
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
		for(int j = 0; j < CLIENT_LIMIT; j++)
			Upipe->sender[i].to[j] = -1;
	
	signal(SIGINT, signalhandler);
	signal(SIGUSR1, signalhandler);
	signal(SIGUSR2, signalhandler);
	////////////////////////////////////////////////////////////////

	/////////////////basic env for each shell///////////////////////
	setenv("PATH", "bin:.", 1); 
	string cmd;
	stringstream sscmd;
	////////////////////////////////////////////////////////////////
	//listernig loop.
	while(true){

		client_fd = accept(socket_fd, (struct sockaddr*) &clientInfo, &addrlen);
		cout << "got client !" << endl;
		pid_t replica_pid = fork();
		if(replica_pid == -1){
			cout << "fork failed" << endl;

		//listrning server.
		}else if(replica_pid > 0){
			char IP[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(clientInfo.sin_addr), IP, INET_ADDRSTRLEN);
			int port =  htons(clientInfo.sin_port);
			CList_shm.register_client(replica_pid, IP, port);
			close(client_fd);

		//client
		}else if(replica_pid == 0){
			dup2(client_fd, STDIN_FILENO);
			dup2(client_fd, STDOUT_FILENO);
			dup2(client_fd, STDERR_FILENO);
			close(client_fd);

			cout <<"****************************************\n** Welcome to the information server. **\n****************************************" << endl;
			broadcast(MEMBER_JOIN, NULL);

			//char input_buffer[INPUT_BUFFER_SIZE]= {0};
			string cmd;
			char temp[INPUT_BUFFER_SIZE];
			while(true){
				cout << "% " << flush;
				//get_cmd(input_buffer);
				//read(STDIN_FILENO, input_buffer, sizeof(input_buffer) );
				cmd.clear();
				getline(cin,cmd);
				
				int k =0;
				memset( temp, '\0', sizeof(temp) );
				strcpy(temp, cmd.c_str()); 
				for(int i = 0; i< sizeof(temp); i++ ){
					if(temp[i] == '\r'){
						continue;
					}else if(temp[i] == '\n'){
						continue;
					}else{
						cmd[k] = temp[i];
						k++;
					}
					if(i = strlen(temp))
						break;
				}
				//cmd = input_buffer;
				sscmd.str("");
				sscmd.clear();
				sscmd.str(cmd);
				if( !isSpecial_cmd(sscmd)){
					
					if(shell_exit == true){
						break;
					}else if(isChat_cmd(sscmd) ){
						//no op if not chat_cmd continue
					}else{
						parse_cmd(sscmd);
					}
				}
			}
			broadcast(MEMBER_LEAVE, NULL);
			Client_List[get_client_id(getpid())].init();
			
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			exit(-1);
		}
	}


}
void get_cmd(char* output_cmd){
	
	
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
	vector<string> argv_table = retrieve_argv(chat_cmd);   // parse out cmd before sign |!><
	if (argv_table.empty() ){
		return true;
	}
	if(argv_table.at(0) == "who" ){
		cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
		for(int i = 1; i < CLIENT_LIMIT; i++){
			if(Client_List[i].pid != -1 ){
				if(Client_List[i].pid == getpid() ){
					cout << i << "\t" << Client_List[i].name << "\t" << Client_List[i].IP<< "\t" <<":" << Client_List[i].port<< "\t" <<  "<-me" << endl;
				}else{
					cout << i << "\t" << Client_List[i].name << "\t" << Client_List[i].IP<< ":" <<  Client_List[i].port<<endl;
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
		cout << "name is" << argv_table.at(1) << endl;
		if(isName_exist == true){
			cout << "*** User '" << argv_table.at(1) <<"' already exists. ***"<< endl;
		}else{	
			memset(Client_List[ get_client_id(getpid()) ].name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			Client_List[ get_client_id(getpid()) ].change_name(argv_table.at(1).c_str() ) ;
			broadcast(NAME_CHANGE, NULL);
		}
		return true;
	}
	if(argv_table.at(0) == "yell" ){
		char message[1024] = {0};

		strcat(message, " ");
		cmdline = cmdline.substr(5,cmdline.length()) ;
		strcat(message, cmdline.c_str());
		broadcast(MEMBER_YELL, message);
		return true;
	}				
	
	if(argv_table.at(0) == "tell" ){
		if(!isUserExist(   atoi( argv_table.at(1).c_str()  )      )     ){
			cout << "*** Error: user #" << argv_table.at(1).c_str() << " does not exist yet. ***" << endl;
			return true;
		}
		char message[1024] = "*** ";
		strcat(message, Client_List[ get_client_id(getpid()) ].name);
		strcat(message, " told you ***: ");
		cmdline = cmdline.substr(7,cmdline.length()) ;
		strcat(message, cmdline.c_str());

		MSG_shm.write_msg(message);	
		kill(Client_List[atoi(argv_table.at(1).c_str() )].pid, SIGUSR1);
		return true;
	}	
	return false;;
}

void broadcast( int action, char* msg){
	char broadcast_content[1024];
	memset(broadcast_content,0,strlen(broadcast_content));
	cout << "ini braodcast is: " << broadcast_content << endl;
	switch(action){
		case MEMBER_JOIN:
			sprintf(broadcast_content,"*** User '%s' entered from %s:%d. ***",Client_List[get_client_id(getpid())].name, 
																			  Client_List[get_client_id(getpid())].IP,
																			  Client_List[get_client_id(getpid())].port);
		break;
		case MEMBER_LEAVE:
			sprintf(broadcast_content,"*** User '%s' left. ***", Client_List[get_client_id(getpid())].name);
		break;
		case NAME_CHANGE:
			sprintf(broadcast_content,"*** User from %s:%d is named '%s'.***", Client_List[get_client_id(getpid())].IP,
																			   Client_List[get_client_id(getpid())].port,
																			   Client_List[get_client_id(getpid())].name);
		break;
		case MEMBER_YELL:
			sprintf(broadcast_content,"*** %s yelled ***: %s", Client_List[get_client_id(getpid())].name, msg);
		break;
		case USER_PIPE_SENDER:
			sprintf(broadcast_content,"***  %s (#%d) just piped '%s' to %s (#%d) ***", Client_List[get_client_id(getpid())].name, get_client_id(getpid()),
																				  		msg, Client_List[Upipe_receiever].name, Upipe_receiever);
			cout << "id is: " << get_client_id(getpid()) << "name is: " <<Client_List[get_client_id(getpid())].name << endl;
		break;
		case USER_PIPE_RECIEVER:
			sprintf(broadcast_content,"***  %s (#%d) just received from %s (#%d) by %s' ***", Client_List[get_client_id(getpid())].name, get_client_id(getpid()),
																				  			  Client_List[Upipe_sender].name, Upipe_sender, msg);
		break;
	}
	cout << "         braodcast is" << broadcast_content << endl;
	MSG_shm.write_msg(broadcast_content);
	for(int j= 1; j < CLIENT_LIMIT; j++){			
		if(Client_List[j].pid  !=  -1 ){  
			kill(Client_List[j].pid, SIGUSR1);
		}
	}
}


int get_client_id(int pid){
	for(int i = 1; i < CLIENT_LIMIT; i++){
		if(((Client_state *)Client_List)[i].pid == pid ){
			return i;
		}
	}
	return -1;
}


void signalhandler(int signo){
	switch(signo){
		case SIGCHLD:
			int status;
			while (waitpid(-1, &status, WNOHANG) > 0);
		break;
		case SIGINT:
			shmdt(CList_shm.attach_ptr);	//shm detach
			shmctl(CList_shm.shmid,IPC_RMID,0);	//close shm
			shmdt(MSG_shm.attach_ptr);	//shm detach
			shmctl(MSG_shm.shmid,IPC_RMID,0);	//close shm
			shmdt(Upipe_shm.attach_ptr);	//shm detach
			shmctl(Upipe_shm.shmid,IPC_RMID,0);	//close shm
			close(socket_fd);
			exit(1);
		break;
		case SIGUSR1:	//get broadcast and tell msg.
			cout << shm_msg << endl;
		break;
		case SIGUSR2:	//get Upipe sig.
			for(int i = 1; i < CLIENT_LIMIT; i++){
				if(read_record[i] == 0 && Upipe->sender[i].to[get_client_id(getpid())] == true ){	
					memset(upipe_name,0,sizeof(upipe_name));
					sprintf(upipe_name,"%dto%d",i, get_client_id(getpid()) );
					read_record[i] = open(upipe_name,O_RDONLY); 
					Upipe->sender[i].rfd[get_client_id(getpid())] = read_record[i];
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
	bool pipe_flag;	
	bool create_user_pipe_flag;
	bool getUpipe_success;
	bool shockMarckflag;
	bool file_flag;
	bool target_flag;
	bool Upipe_err_flag; 
	Pipe_class current_pipe_record;
	Pipe_class pipe_reached_target;

	while( !sscmd.eof()){      			//check sstream of cmdline is not eof.
		//unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
		int newProcessIn = STDIN_FILENO;   //shell process's fd 0,1,2 are original one. never changed.
 		int newProcessOut = STDOUT_FILENO;
		int newProcessErr = STDERR_FILENO;
		pipe_flag = false;
		create_user_pipe_flag = false;
		getUpipe_success = false;
		shockMarckflag = false;
		file_flag	= false;
		target_flag = false;
		Upipe_err_flag = false;

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

				if( read_record[pipenumber] != 0 && Upipe->sender[pipenumber].to[get_client_id(getpid() )] == true ){
					getUpipe_success = true;
					newProcessIn = read_record[pipenumber]; 
					Upipe_sender = pipenumber;
					//reset upipe share.
					argv_table.pop_back();
					read_record[pipenumber] =0;
					Upipe->sender[pipenumber].to[get_client_id(getpid() )] = false;
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
					cout <<"*** Error: the pipe #" << pipenumber << "->#" << get_client_id(getpid() ) << " does not exist yet. ***" << endl;
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
				//cout << "I am piping " << endl;
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
		
		int Upipe_read_tmp;   //L 663
		int Upipe_write_tmp;	// L664
		bool double_Upipe = false;
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
					cout << "*** Error: user #" << pipenumber << " d does not exist yet. ***" << endl;
					newProcessIn = open("/dev/null", O_RDWR);
					newProcessOut = newProcessIn;
				}
				//check if user_pipe exist.
				if( Upipe->sender[get_client_id(getpid() )].to[pipenumber] == true ){
					Upipe_err_flag = true;
					cout << "*** Error: the pipe #"<< get_client_id(getpid() )<<">#"<< pipenumber <<" already exists. ***" << endl;
					newProcessIn = open("/dev/null", O_RDWR);
					newProcessOut = newProcessIn;
				}
				//user pipe is avaliable to create.
				if(Upipe_err_flag == false){
					create_user_pipe_flag = true;
					Upipe->sender[get_client_id(getpid() )].to[pipenumber] = true;
					Upipe_receiever = pipenumber;


					memset(upipe_name,0,sizeof(upipe_name));
					sprintf(upipe_name,"%dto%d",get_client_id(getpid()), pipenumber );
					mkfifo(upipe_name, 0644);
					kill(Client_List[pipenumber].pid, SIGUSR2);
					newProcessOut = open(upipe_name, O_WRONLY);

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
				//cout << " I am target" << endl;
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
				//cout << "fork: pipe_flag == true, close: " << current_pipe_record.get_read() << ", " << newProcessOut<< endl;
				close(current_pipe_record.get_read());			//it suppose to close read of new pipe.  If other data want to use the same pipe, STD_read would be useless.
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
			//if there is no fd change,  eveytime child fork starts with shell_std_in/out/err.  e.q. isolated cmd like ls in "cat file |2 ls number"
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
			}
			if(target_flag == true){
				close(pipe_reached_target.get_read());
				pipe_vector.erase(pipe_vector.begin()+pop_out_index);
			}
			if(create_user_pipe_flag){
				close(newProcessOut);
			}
			if(STDOUT_FILENO == newProcessOut || file_flag == true ){		//command pid want to printout to console. or wait writting to file.
				int status;															
				waitpid(pid, &status, 0);
			}
			//cout << "pipe_vector size at the end: " << pipe_vector.size() << endl;
			
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
		if(parsed_word == "PATH" || parsed_word == "LANG"){
			char* pPath = getenv(parsed_word.c_str());
			if(pPath != NULL)
				cout<<pPath<<endl;
		}
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