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
		void write_msg(char* inut_msg){
			memset((char*)attach_link,0,sizeof(SHARE_MSG_SIZE));
			strcpy((char*)attach_link, inut_msg);
		}
		void creat_shmid(key_t key_temp, int size_temp){
			key = key_temp;
			size = size_temp;
			shmid = shmget( key, size, 0644 | IPC_CREAT);
			if (shmid == -1){
				fprintf (stderr, "shmget failed\n");
				exit (1);
			}
		}
		void* attatch_shm(){
			attach_link = shmat(shmid, NULL, 0);		
			if (attach_link == (void *)-1){
				fprintf (stderr, "shmat failed\n");
				exit (1);
			}
			return attach_link;
		}
		void Clist_init(){
			for(int i = 0; i < CLIENT_LIMIT; i++){	  //start from 0, in case of error.
				((Client_state*)attach_link)[i].init();
			}
		}
		int register_client(int input_pid, char input_ip[INET_ADDRSTRLEN] , unsigned short int input_port){
			Client_state* Client_List = (Client_state*)attach_link;
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
		//===========member==============
		void* attach_link = NULL;
		key_t key;
		int shmid;
		int size;
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
/////////Global Variable//////////////
Sh_mem CList_shm, MSG_shm;
Client_state *Client_List;
char *shm_msg;
bool shell_exit = false;
///////////////////////////////////////
vector<string> retrieve_argv(stringstream&);



int main(int argc, char* argv[]){
	
	///////////////////////conndection////////////////////////////////
	int socket_fd =0;
	int client_fd = 0;
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
	MSG_shm.creat_shmid(SHARE_MSG_KEY, SHARE_MSG_SIZE); 
	shm_msg = (char*)MSG_shm.attatch_shm();	
	memset(shm_msg,0,sizeof(SHARE_MSG_SIZE));

	CList_shm.creat_shmid(CLIST_KEY, CLIENT_LIMIT*sizeof(Client_state));
	Client_List = (Client_state *)CList_shm.attatch_shm();
	CList_shm.Clist_init();

	signal(SIGINT, signalhandler);
	signal(SIGUSR1, signalhandler);
	////////////////////////////////////////////////////////////////

	//listernig loop.
	while(true){

		client_fd = accept(socket_fd, (struct sockaddr*) &clientInfo, &addrlen);
		
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
			usleep(10000);
			broadcast(MEMBER_JOIN, NULL);
			setenv("PATH", "bin:.", 1) ; 
			string cmd;
			stringstream sscmd;
			char input_buffer[INPUT_BUFFER_SIZE]= {0};
			while(true){
				cout << "% " << flush;
				get_cmd(input_buffer);
				cmd = input_buffer;
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
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			exit(-1);
		}
	}


}
void get_cmd(char* output_cmd){
	
	read(STDIN_FILENO, output_cmd, sizeof(output_cmd) );
	char temp[INPUT_BUFFER_SIZE];
	int k =0;
	strcpy(temp, output_cmd); 
	memset( output_cmd, '\0', sizeof(output_cmd) );
	for(int i = 0; i< sizeof(temp); i++ ){
		if(temp[i] == '\r'){
			continue;
		}else if(temp[i] == '\n'){
			continue;
		}else{
			output_cmd[k] = temp[i];
			k++;
		}
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
	vector<string> argv_table = retrieve_argv(chat_cmd);   // parse out cmd before sign |!><
	if (argv_table.empty() ){
		return true;
	}
	//===============ChatRoom cmd================
	if(argv_table.at(0) == "who" ){
		cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
		for(int i = 1; i < CLIENT_LIMIT; i++){
			if(Client_List[i].pid != -1 ){
				if(Client_List[i].pid == getpid() ){
					cout << i << "\t" << Client_List[i].name << "\t" << Client_List[i].IP<< "\t" <<":" << Client_List[i].port<< "\t" <<  "<-me" << endl;
				}else{
					cout << i << "\t" << Client_List[i].name << "\t" << Client_List[i].IP<< ":" <<  Client_List[i].port<< "\t" << endl;
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
			printf("*** User '%s' already exists. ***\n",argv_table.at(1).c_str());
		}else{	
			memset(Client_List[ get_client_id(getpid()) ].name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(Client_List[ get_client_id(getpid()) ].name, argv_table.at(1).c_str(), argv_table.at(1).length()  );
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
		/*
		memset(shm_msg,0,sizeof(shm_msg));		//sizeof shm msg seems not 15000
		strcpy(shm_msg, message);
		*/
		MSG_shm.write_msg(message);
		kill(Client_List[atoi(argv_table.at(1).c_str() )].pid, SIGUSR1);
		return true;
	}	
	return false;;
	//===============ChatRoom cmd  end================
}

void broadcast( int action, char* msg){
	stringstream ss;
	string broadcast_content;
	ss.str("");
	ss.clear();
	switch(action){
		case MEMBER_JOIN:
			ss <<  "*** User \'" << Client_List[get_client_id(getpid())].name;
			ss << "\' entered from ";
			ss << Client_List[get_client_id(getpid())].IP; ss << ":"; ss << Client_List[get_client_id(getpid())].port;
			ss << ". ***";
			broadcast_content = ss.str();
			break;
		case MEMBER_LEAVE:
			ss << "*** User \'" << Client_List[get_client_id(getpid())].name; ss << "\' left. ***" ;
			broadcast_content = ss.str();
	
			break;
		case NAME_CHANGE:
			ss << "*** User from "; ss << Client_List[get_client_id(getpid())].IP; ss << ":"; ss << Client_List[get_client_id(getpid())].port;
			ss << " is named \'"  ; ss << Client_List[get_client_id(getpid())].name;	 ss << "\'. ***";
			broadcast_content = ss.str();
			break;
		case MEMBER_YELL:
			ss << "*** " << Client_List[get_client_id(getpid())].name;  ss << " yelled ***:";  ss << msg;
			broadcast_content = ss.str();
			break;
		/*
		case USER_PIPE_SENDER:
			ss << "*** "; ss << Client_List[get_client_id(getpid())].name; ss << " (#"; ss << Client_List[get_client_id(getpid())].get_charID(); ss << ") ";
			ss << "just piped \'"; ss << msg; ss << "\' to ";
			ss << Client_List[Client_List[get_client_id(getpid())].ID_to_reciever].client_name; ss << " (#"; ss << Client_List[Client_List[get_client_id(getpid())].ID_to_reciever].get_charID();
			ss << ") ***\n";
			broadcast_content = ss.str();
			break;
		case USER_PIPE_RECIEVER:
			ss << "*** "; ss << Client_List[get_client_id(getpid())].name; ss << " (#"; ss << Client_List[get_client_id(getpid())].get_charID(); ss << ") ";
			ss << "just received from "; ss << Client_List[ Client_List[get_client_id(getpid())].ID_from_sender  ].client_name; ss << " (#"; ss << Client_List[ Client_List[get_client_id(getpid())].ID_from_sender ].get_charID();
			ss << ") by \'";  ss << msg;  ss << "\' ***\n";
			broadcast_content = ss.str();
			break;
		*/
	}
	memset(shm_msg,0,sizeof(shm_msg));
	strcpy(shm_msg, broadcast_content.c_str());
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
			shmdt(CList_shm.attach_link);	//shm detach
			shmctl(CList_shm.shmid,IPC_RMID,0);	//close shm
			shmdt(MSG_shm.attach_link);	//shm detach
			shmctl(MSG_shm.shmid,IPC_RMID,0);	//close shm
			exit(-1);
		break;
		case SIGUSR1:
			cout << shm_msg << endl;
		break;
		case SIGUSR2:
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
	bool pipe_create_flag;
	bool shockMarckflag;
	bool file_flag;
	bool target_flag;
	bool unknown_cmd = false;     //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
	Pipe_class current_pipe_record;
	Pipe_class pipe_reached_target;

	while( !sscmd.eof() && !unknown_cmd){      			//check sstream of cmdline is not eof.
		//unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
		int newProcessIn = STDIN_FILENO;   //shell process's fd 0,1,2 are original one. never changed.
 		int newProcessOut = STDOUT_FILENO;
		int newProcessErr = STDERR_FILENO;
		pipe_flag = false;
		pipe_create_flag = false;
		shockMarckflag = false;
		file_flag	= false;
		target_flag = false;
		unknown_cmd = false;
		string teststring = sscmd.str();
		//cout <<"string test in parsing " << teststring << endl;
		vector<string> argv_table;
		
		argv_table = retrieve_argv(sscmd);   // parse out cmd before sign |!>

		if (argv_table.empty())
			break;
		string sign_number;
		char *pch;
		int pipnumber = 0;
		sscmd >> sign_number;
		int test = 0;
		switch(sign_number[0]){
		case '|':		
			 //may not creat a pipe, need to check if target the same as previous process. 
			//if so, store current_numPipe_at   in global vector.
			pipe_flag = true;
			if( isdigit( sign_number[1]) ){			//don't pop sign, in case of '|' at the end of cmd.
				sign_number = sign_number.substr(1); //skip the first charactor.	
				replace(sign_number.begin(), sign_number.end(), '+', ' ');
				stringstream numb(sign_number);
				while (!numb.eof()){
					numb >> test;
					pipnumber += test;
				}
				/*
				 //version 2 which fucked me 
				char temp[sign_number.length()+1];
				strcpy(temp,sign_number.c_str());
				pch = strtok(temp,"+");
				while(pch != NULL){
					test = atoi(pch);
					pipnumber += test;
					pch = strtok(NULL,"+");
				}
				*/
				current_pipe_record.set_numPipe_count(pipnumber);
			} else {
				current_pipe_record.set_numPipe_count(1);
			}
			break;
		case '!':
			//pratically the same as mentioned ahead.
			// creat a pipe and numbPipe_at number. stored in global vector.	
			pipe_flag = true;
			shockMarckflag = true;
			if( isdigit( sign_number[1]) ){			//don't pop sign, in case of '|' at the end of cmd.
				sign_number = sign_number.substr(1);				//skip the first charactor.		
				current_pipe_record.set_numPipe_count(atoi(sign_number.c_str()));
			} else {
				current_pipe_record.set_numPipe_count(1);
			}
			break;
		case '>':
			file_flag = true;
			string filename;
			sscmd >> filename;
			newProcessOut = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC , 00777);
			break;
		}
		//if upcomming child process is target or not.
		int target_flag = false;
		int pop_out_index = -1;
		for(int i = 0; i< pipe_vector.size(); i++){
			if(pipe_vector[i].get_count() == 0){
				target_flag = true;
				pop_out_index = i;
				newProcessIn = pipe_vector[i].get_read();
				pipe_reached_target = pipe_vector[i];
				//write to pipe which arrives target will be useless. it'll be closed in both child and parent process.
				close(pipe_vector[i].get_write() );
			}
			//can not decrease vector here. itll influence following Pioneer part.
		}

		// if it's not target, and it's doesn't creat pipe, 
		//the process in/out/err will remain shell_in/out/err_reserved.
		// for case  "cat test.html |2 ls", in which "ls" printout in shell.

		//check if upcomming process_output has the same target with any current_pipe_record's.
		int isPioneer = true;
		if(pipe_flag == true){
			for(int i = 0; i< pipe_vector.size(); i++){
				if(current_pipe_record.get_count() == pipe_vector[i].get_count()){
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

		while ( (pid = fork()) < 0){
			usleep(100);
		}
		if( pid == 0){	 											// child

			if(target_flag == true){
				dup2(newProcessIn, STDIN_FILENO);  //input stream never be the same as STDIN_NO
				close(newProcessIn);

			}
			if(pipe_flag == true){
				close(current_pipe_record.get_read());
				dup2(newProcessOut, STDOUT_FILENO);  //output stream never be the same as STDOUT_NO
				if(shockMarckflag == true ){
					dup2(newProcessErr, STDERR_FILENO);
				}
				close(newProcessOut);
			} else if(file_flag == true ) {
				dup2(newProcessOut, STDOUT_FILENO);	
				close(newProcessOut);
			} 
			//if there is no fd change,  eveytime child fork starts with shell_std_in/out/err
			//e.q. isolated cmd like ls in "cat file |2 ls number"

			//get argv.			
			const char *pargv[argv_table.size() + 1];
			convert_argv_to_consntchar(pargv, argv_table);
			//cout << pargv[0] << endl;
			//cout << pargv[1] << endl;
			//cout << "argv_table.size() is: " << argv_table.size() << endl;
			execvp(pargv[0], (char **) pargv);
			if(execvp(pargv[0], (char **) pargv) == -1 ){
				fprintf(stderr,"Unknown command: [%s].\n",pargv[0]);
				unknown_cmd = true; //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
			}
			exit(-1);

			
		} else if(pid >0 ){			//parent
			
			signal(SIGCHLD, signalhandler);
			for(int i = 0; i< pipe_vector.size(); i++){
				pipe_vector[i].count_decrease();
			}
			if(file_flag == true){
				close(newProcessOut);
			}
			if(target_flag == true){
				//pipe_reached_target.close_pipe();
				close(pipe_reached_target.get_read());
				pipe_vector.erase(pipe_vector.begin()+pop_out_index);
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