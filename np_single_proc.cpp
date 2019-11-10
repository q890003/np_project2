#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>		
#include <cstring>		//strchr()
#include <vector>
#include <algorithm>    // 														std::replace
#include <stdio.h>  		// 																			fprint error
#include <stdlib.h>		//																							setenv
#include <unistd.h>		// wait(),exec(), pipe(), fork()  select()
#include <sys/types.h> //									fork() 
#include <sys/wait.h>  //	wait()
#include <sys/socket.h>
#include <sys/time.h>	// 											select()
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>		//open()
//#include <string.h>	//for version2 at line 135-145 strtok
//#include <stdlib.h>	//for version2 at line 135-145 strtok
using namespace std;
#define CLIENT_LIMIT 31
#define CLIENT_NAME_SIZE 20
#define PORT_SIZE	6
#define MEMBER_JOIN 0
#define MEMBER_LEAVE 1
#define NAME_CHANGE 2
#define MEMBER_YELL 3
#define USER_PIPE_SENDER 4
#define USER_PIPE_RECIEVER 5

//===Chatting Room====
void broadcast(int, int, char*);  		//(int client_ID, int Action)
char Welcome[] = "***************************************\n** Welcome to the information server **\n***************************************\n";
char prompt[] = "% ";
//==================



//=======parsing======
void childHandler(int);
void convert_argv_to_consntchar(const char, vector<string> );
int parse_cmd(int, stringstream &);
bool special_cmd(stringstream&);
bool shell_exit = false;
vector<string> retrieve_argv(stringstream&);
//==================

class Pipe_class{
	public: 
	Pipe_class(){
		isUserPipe = false;
	}
	int get_read(){
		return pfd[0];
	}
	int get_write(){
		return pfd[1];
	}
	int set_read(int fd){
		pfd[0] = fd;
	}
	int set_write(int fd){
		pfd[1] = fd;
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
		if(isUserPipe == true)
			cout << "pipe is closing" << endl;

		close(pfd[0]);
		close(pfd[1]);
	}
		int pfd[2];
		int numPipe_count;
		int client_ID;		//a kind of pipe for private user use.
		int sender;		//a pipe between user.
		int receiver;
		bool isUserPipe;
		vector<Pipe_class>::iterator user_pipe_record;
};
class Client_state{
	public:
		void Client_state_init(){
			client_fd = -1;
			client_ID = -1;
			strncpy(client_name,"(no name)",strlen("(no name)") );
		}
		void reset(){
			client_fd = -1;
			client_ID = -1;
			memset( client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(client_name,"(no name)",strlen("(no name)") );
			memset( IP, 0, INET_ADDRSTRLEN*sizeof(char) );
			port = 0;
			ID_from_sender = -1;
			ID_to_reciever = -1;
		};
		void change_name(const char* newname){
			memset( client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(client_name, newname, strlen(newname) );
		}
		char* get_charID(){
			sprintf(charID,"%d",client_ID);
			return charID;
		}
		int client_fd;
		int client_ID;
		char charID[16];
		char client_name[CLIENT_NAME_SIZE];
		char IP[INET_ADDRSTRLEN];
		unsigned short int port;									//not sure about port size.
		int ID_from_sender;
		int ID_to_reciever;

		//Pipe_class user_pipe;

};
vector<Pipe_class> pipe_vector;					//need to revise//////////////////////////////////////////////////
Client_state Client_manage_list[CLIENT_LIMIT] ;

int main(int argc, char* argv[]){
	
	setenv("PATH", "bin:.", 1) ; 


	for(int i=0 ; i< CLIENT_LIMIT; i++){
		Client_manage_list[i].Client_state_init();
	}
	fd_set master;
	fd_set rfds;	//read fd set	//it's tempary.
	int fdmax = 0;
	
	int listener;
	int newfd;
	//struct sockaddr_storage client_addr;
	socklen_t addrlen;
	
	char input_buffer[1024]= {0};
	int nbytes;
	int rv;		//don't know yet/////////////////////////////////
	struct addrinfo hints, *ai, *p;		
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;				//it's not fixed. give elastic option.
	hints.ai_socktype = SOCK_STREAM;	//stream: connect oriented.
	hints.ai_flags = AI_PASSIVE;				//don't know yet/////////////////////////////////
													//argv[1] is port setted when execute the program.   '80' usually used for http .etc.
	if( (rv = getaddrinfo(NULL, argv[1], &hints, &ai) ) != 0){			//getinfo helps to confige pre setting. three input and 1 output. if success,  return 0.
		fprintf(stderr, "selectserver: %s !!!!!!!!\n", gai_strerror(rv));		//don't know yet/////////////////////////////////
		exit(1);				//don't know yet/////////////////////////////////
	}
	listener = socket(AF_INET, SOCK_STREAM, 0);		//AF_INET is IPV4. 
	if (listener == -1){
        cout <<"Fail to create a socket." << endl;
    }
	
	struct sockaddr_in serverSockInfo,clientSockInfo;
	addrlen = sizeof(clientSockInfo);
	bzero(&serverSockInfo,sizeof(serverSockInfo));  //initiialize server info.
	bzero(&serverSockInfo,sizeof(clientSockInfo));  //initiialize client info.
	serverSockInfo.sin_family = PF_INET;
    serverSockInfo.sin_addr.s_addr = INADDR_ANY;
    serverSockInfo.sin_port = htons(atoi(argv[1]));
	bind(listener, (struct sockaddr *)&serverSockInfo, sizeof(serverSockInfo));
	
	listen(listener, CLIENT_LIMIT);
	FD_SET(listener, &master);
	fdmax = listener;
	int SERVER_STDIN_FD = dup(STDIN_FILENO);
	int SERVER_STDOUT_FD = dup(STDOUT_FILENO);
	int SERVER_STDERR_FD = dup(STDERR_FILENO);
	int serving_to_client_fd = -1;
	int serving_client_id = -1;
	while(true){
		 rfds = master; // update rfds from master.

		while(select(fdmax+1, &rfds, NULL, NULL, NULL) <= -1) {
			cout << "sth happen to select" << endl;
			//perror("[error] select");
			//exit(4);
		}
		for(int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &rfds)) {
				//new connection 
				if (i == listener) {
					newfd = accept(listener, (struct sockaddr *)&clientSockInfo,	&addrlen);		//client_addr what's diffferent storage and socearr?
					if(newfd == -1){
						cout << "fail of createing client_fd" << endl;
					//fd create succeed.
					}else{
						serving_to_client_fd = newfd;
						send(newfd, Welcome, sizeof(Welcome), 0 );
						
						for(int j= 0; j <CLIENT_LIMIT; j++){			
							if(Client_manage_list[j].client_ID  ==  -1 ){
								//char charID[25];
								Client_manage_list[j].client_ID =  j;
								Client_manage_list[j].client_fd = newfd;
								//sprintf(charID,"%d",j);
								//Client_manage_list[j].client_name += charID;
								inet_ntop(AF_INET, &(clientSockInfo.sin_addr), Client_manage_list[j].IP, INET_ADDRSTRLEN);
								Client_manage_list[j].port = clientSockInfo.sin_port;
								broadcast(Client_manage_list[j].client_ID, MEMBER_JOIN, NULL);
								cout << Client_manage_list[j].IP << " is connect" << endl;
								break;
							}
						}
						FD_SET(newfd, &master);
						if(newfd > fdmax){
							fdmax = newfd;
						}

					}
				}else{
					serving_to_client_fd = i;
					for(int j = 0; j < CLIENT_LIMIT; j++){
						if(Client_manage_list[j].client_fd == i){
							serving_client_id = j;
						}
					}
					
					nbytes = recv(serving_to_client_fd, input_buffer, sizeof(input_buffer), 0);	//not handle recv <0 yet.
					//handling '\r' from telnet.
					char* pch = strchr(input_buffer,'\r');		
					if( pch != NULL){
						*pch = '\0';
					}
					string cmd;
					cmd = input_buffer;
					if( nbytes ==0 || cmd =="exit"){
						//connection aborted
						broadcast(Client_manage_list[serving_client_id].client_ID, MEMBER_LEAVE, NULL);
						for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it < pipe_vector.end(); ++it){
							if( (*it).client_ID == serving_client_id || (*it).sender == serving_client_id){			
								(*it).close_pipe();
								pipe_vector.erase(it);
							}
						}
						Client_manage_list[serving_client_id].reset();
						close(i);
						FD_CLR(i, &master);
					}else{	//there is data comming
						stringstream sscmd(cmd);
						
						if( !special_cmd(sscmd)){
							if(shell_exit == true){
								//no op
							}else{
								dup2(serving_to_client_fd, STDIN_FILENO);
								dup2(serving_to_client_fd, STDOUT_FILENO);
								dup2(serving_to_client_fd, STDERR_FILENO);
								parse_cmd(serving_client_id, sscmd);
								dup2(SERVER_STDIN_FD, STDIN_FILENO);
								dup2(SERVER_STDOUT_FD, STDOUT_FILENO);
								dup2(SERVER_STDERR_FD, STDERR_FILENO);
							}
						}
					}
				}
			usleep(10000);
			send(serving_to_client_fd, prompt, sizeof(prompt), 0);
			}
		}
	}
	return 0;
}

void broadcast(int ID_num, int action, char* msg){
	stringstream ss;
	string broadcast_content;
	switch(action){
		case MEMBER_JOIN:
			ss <<  "*** User \'" << Client_manage_list[ID_num].client_name;
			ss << "\' entered from ";
			ss << Client_manage_list[ID_num].IP;
			ss << ". ***\n";
			broadcast_content = ss.str();
			break;
		case MEMBER_LEAVE:
			ss << "*** User \'" << Client_manage_list[ID_num].client_name;
			ss << "\' left. ***\n" ;
			broadcast_content = ss.str();
			break;
		case NAME_CHANGE:
			ss << "** User from ";
			ss << Client_manage_list[ID_num].IP;
			ss << ":";
			ss << Client_manage_list[ID_num].port;
			ss << " is named \'";
			ss << Client_manage_list[ID_num].client_name;
			ss << "\'. ***\n";
			broadcast_content = ss.str();
			break;
		case MEMBER_YELL:
			ss << "*** " << Client_manage_list[ID_num].client_name;
			ss << " yelled ***:";
			ss << msg;
			broadcast_content = ss.str();
			break;
		case USER_PIPE_SENDER:
			ss << "*** ";
			ss << Client_manage_list[ID_num].client_name; ss << " (#"; ss << Client_manage_list[ID_num].get_charID(); ss << ") ";
			ss << "just piped \'"; ss << msg; ss << " >"; ss << Client_manage_list[ID_num].ID_to_reciever ; ss << "\' to ";
			ss << Client_manage_list[Client_manage_list[ID_num].ID_to_reciever].client_name; ss << " (#"; ss << Client_manage_list[Client_manage_list[ID_num].ID_to_reciever].get_charID();
			ss << ") ***\n";
			broadcast_content = ss.str();
			break;
		case USER_PIPE_RECIEVER:
			ss << "*** ";
			ss << Client_manage_list[ID_num].client_name; ss << " (#"; ss << Client_manage_list[ID_num].get_charID(); ss << ") ";
			ss << "just received from ";
			ss << Client_manage_list[ Client_manage_list[ID_num].ID_from_sender  ].client_name; ss << " (#"; ss << Client_manage_list[ Client_manage_list[ID_num].ID_from_sender ].get_charID();
			ss << ") by \'";
			ss << msg;
			ss << " <";
			ss << Client_manage_list[Client_manage_list[ID_num].ID_from_sender].get_charID();
			ss << "\' ***\n";
			broadcast_content = ss.str();
			break;
	}
	for(int j= 0; j <CLIENT_LIMIT; j++){			
		if(Client_manage_list[j].client_ID  !=  -1 ){  
			send(Client_manage_list[j].client_fd, broadcast_content.c_str(), broadcast_content.length(), 0 );
		}
	}
}


void childHandler(int signo){
	int status;
	while (waitpid(-1, &status, WNOHANG) > 0);
}
void convert_argv_to_consntchar(const char *argv[], vector<string> &argv_table) {
	for (int i=0; i<argv_table.size(); i++) {
		argv[i] = argv_table[i].c_str();
	}
	argv[argv_table.size()] = NULL;
}
int parse_cmd(int serving_client_id, stringstream &sscmd){
	bool pipe_flag;	
	bool create_user_pipe_flag;
	bool user_pipe_reciever_flag;
	//bool pipe_create_flag;
	bool shockMarckflag;
	bool file_flag;
	bool target_flag;
	bool unknown_cmd = false;     //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
	Pipe_class current_pipe_record;
	Pipe_class pipe_reached_target;

	while(!sscmd.eof() && !unknown_cmd){      			//check sstream of cmdline is not eof.
		//unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
		int newProcessIn = STDIN_FILENO;   //shell process's fd 0,1,2 are client's, who gives cmd, std in/out/err.
 		int newProcessOut = STDOUT_FILENO;
		int newProcessErr = STDERR_FILENO;
		pipe_flag = false;
		create_user_pipe_flag = false;
		user_pipe_reciever_flag = false;
		//pipe_create_flag = false;
		shockMarckflag = false;
		file_flag	= false;
		target_flag = false;
		unknown_cmd = false;
		
		vector<string> argv_table = retrieve_argv(sscmd);   // parse out cmd before sign |!><
		if (argv_table.empty()){
			break;
		}
		//===============ChatRoom cmd================
		if(argv_table.at(0) == "who" ){
			cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me> "<<endl;
			for(int  ID_transverse= 0; ID_transverse <CLIENT_LIMIT; ID_transverse++){
				if(Client_manage_list[ID_transverse].client_ID != -1){
					if(serving_client_id == ID_transverse ){
						cout <<Client_manage_list[ID_transverse].client_ID <<"\t"<< Client_manage_list[ID_transverse].client_name <<"\t" <<Client_manage_list[ID_transverse].IP <<":"<<Client_manage_list[ID_transverse].port  << "\t<- me" <<endl;
					}else{
						cout <<Client_manage_list[ID_transverse].client_ID <<"\t"<< Client_manage_list[ID_transverse].client_name <<"\t" << Client_manage_list[ID_transverse].IP <<":"<<Client_manage_list[ID_transverse].port << endl;
					}
				}
			}
			break;		//not sure if there would be a bug/
		}
		
		if(argv_table.at(0) == "name" ){
			//Client_manage_list[j].change_name(argv_table.at(1).c_str());
			//check if name already exist or not.
			bool check_name_existence = false;
			for(int  ID_transverse= 0; ID_transverse <CLIENT_LIMIT; ID_transverse++){
				if(strcmp(argv_table.at(1).c_str(), Client_manage_list[ID_transverse].client_name) == 0 ){
					check_name_existence = true;
				}
			}
			if(check_name_existence == true){
				string name_exist = "*** User \'";
				name_exist += argv_table.at(1) + "\' already exists. ***";
				cout << name_exist << endl;
				return 0;
			}else{
				memset(Client_manage_list[serving_client_id].client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
				strncpy(Client_manage_list[serving_client_id].client_name, argv_table.at(1).c_str(), argv_table.at(1).length()  );
				broadcast(serving_client_id, NAME_CHANGE, Client_manage_list[serving_client_id].client_name);
			}
			break;		//not sure if there would be a bug/
		}
		
		if(argv_table.at(0) == "yell" ){
			char message[1024] = {0};
			for(vector<string>::iterator it = argv_table.begin()+1 ; it < argv_table.end(); ++it){
					strcat(message, " ");
					strcat(message, (*it).c_str() );
					}	
			broadcast(Client_manage_list[serving_client_id].client_ID, MEMBER_YELL, message);
			break;		//not sure if there would be a bug/
		}				

		if(argv_table.at(0) == "tell" ){
			bool check_user_existence = false;
			for(int  ID_transverse= 0; ID_transverse <CLIENT_LIMIT; ID_transverse++){
				if(Client_manage_list[ID_transverse].client_ID == atoi( argv_table.at(1).c_str() ) ){
					check_user_existence = true;
				}
			}
			if(check_user_existence == false){
				cout << "*** Error: user #"<< argv_table.at(1) << " does not exist yet. ***" << endl;
				return 0;
			}
			char message[1024] = "*** ";
			strcat(message, Client_manage_list[serving_client_id].client_name);
			strcat(message, " told you ***:");
			for(vector<string>::iterator it = argv_table.begin()+2; it < argv_table.end(); ++it){
				strcat(message, " ");
				strcat(message, (*it).c_str() );
				strcat(message, "\n");
			}
			send(Client_manage_list[atoi( argv_table.at(1).c_str() ) ].client_fd, message, sizeof(message), 0);
			break;		//not sure if there would be a bug/
		}		
		//===============ChatRoom cmd  end================
		
		string sign_number;
		//char *pch;					 //version 2 which fucked me
		int pipnumber = 0;
		sscmd >> sign_number;
		int test = 0;
		switch(sign_number[0]){
		case '|':		
			 //may not creat a pipe, need to check if target the same as previous process. 
			//if so, store current_numPipe_at   in global vector.
			cout << "here I am " << endl;
			current_pipe_record.client_ID = serving_client_id;
			pipe_flag = true;
			if( isdigit( sign_number[1]) ){			//don't pop sign, in case of '|' at the end of cmd.
				sign_number = sign_number.substr(1); //skip the first charactor.	
				replace(sign_number.begin(), sign_number.end(), '+', ' ');
				stringstream numb(sign_number);
				while (numb >> test){
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
			current_pipe_record.client_ID = serving_client_id;
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
			if( isdigit( sign_number[1]) ){
				char user_pipe_msg[1024] ={0};
				create_user_pipe_flag = true;
				sign_number = sign_number.substr(1); //skip the first charactor.	
				replace(sign_number.begin(), sign_number.end(), '+', ' ');
				stringstream numb(sign_number);
				while (numb >> test){
					pipnumber += test;
				}  
				bool check_user_existence = false;
				for(int  ID_transverse= 0; ID_transverse <CLIENT_LIMIT; ID_transverse++){
					if(Client_manage_list[ID_transverse].client_ID == pipnumber){
						check_user_existence = true;
					}
				}
				if(check_user_existence == false){
					cout << "*** Error: user #"<<pipnumber << " does not exist yet. ***" << endl;
					return 0;
				}else{
					string err_msg;
					for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it< pipe_vector.end(); ++it){
						if( ((*it).isUserPipe == true ) && 
							((*it).client_ID == pipnumber) &&
							((*it).receiver == pipnumber ) &&
							((*it).sender == serving_client_id) ){
								err_msg += "*** Error: the pipe #";
								err_msg += Client_manage_list[serving_client_id].get_charID();
								err_msg += "->#";
								err_msg += Client_manage_list[pipnumber].get_charID();
								err_msg += " already exists. ***";
								cout << err_msg << endl;
								return 0;
							}
					}
					Client_manage_list[serving_client_id].ID_to_reciever = pipnumber; //for broadcasting msg.
					current_pipe_record.client_ID = pipnumber;
					current_pipe_record.sender = serving_client_id;
					current_pipe_record.receiver = pipnumber;
					current_pipe_record.isUserPipe = true;
					current_pipe_record.creat_pipe();
					pipe_vector.push_back( current_pipe_record );
					newProcessOut = current_pipe_record.get_write();
					newProcessErr = current_pipe_record.get_write();

					for(vector<string>::iterator it = argv_table.begin(); it < argv_table.end(); ++it){
						if(it != argv_table.begin())
							strcat(user_pipe_msg, " ");
						strcat(user_pipe_msg, (*it).c_str() );
					}
					broadcast(serving_client_id, USER_PIPE_SENDER, user_pipe_msg);
				}
			}else{
				file_flag = true;
				string filename;
				sscmd >> filename;
				newProcessOut = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC , 00777);
			}
			break;
		case '<':
			if( isdigit( sign_number[1]) ){
				sign_number = sign_number.substr(1); //skip the first charactor.	
				replace(sign_number.begin(), sign_number.end(), '+', ' ');
				stringstream numb(sign_number);
				while (numb >> test){
					pipnumber += test;
				}
				
				//check if pipnumber(ID) exist or not.
				bool check_user_existence = false;
				for(int  ID_transverse= 0; ID_transverse <CLIENT_LIMIT; ID_transverse++){
					if(Client_manage_list[ID_transverse].client_ID == pipnumber){
						check_user_existence = true;
					}
				}
				if(check_user_existence == false){
					cout << "*** Error: user #" << pipnumber << " does not exist yet. ***" << endl;
					return 0;
				}else{
					//brocast message.
					user_pipe_reciever_flag = false;		

					for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it< pipe_vector.end(); ++it){
						if( ((*it).isUserPipe == true ) && 
							((*it).client_ID == serving_client_id) &&
							((*it).receiver == serving_client_id ) &&
							((*it).sender == pipnumber) ){
							user_pipe_reciever_flag = true;
							Client_manage_list[serving_client_id].ID_from_sender = pipnumber;	//for braodcast msg.
							current_pipe_record.client_ID = serving_client_id;
							current_pipe_record.receiver = serving_client_id;
							current_pipe_record.sender = pipnumber;
							current_pipe_record.isUserPipe = true;
							current_pipe_record.user_pipe_record = it;
							current_pipe_record.set_read((*it).get_read());
							current_pipe_record.set_write((*it).get_write());
							newProcessIn = (*it).get_read();
							break;
						}
					}
					if(user_pipe_reciever_flag == false){
						string err_msg = "** Error: the pipe #";
						err_msg += Client_manage_list[pipnumber].get_charID();
						err_msg += "->#";
						err_msg += Client_manage_list[serving_client_id].get_charID();
						err_msg += " does not exist yet. ***";
						cout << err_msg << endl;
						return 0;
					}
					char user_pipe_msg[1024] = {0};
					for(vector<string>::iterator it = argv_table.begin(); it < argv_table.end(); ++it){
						if(it != argv_table.begin())
						strcat(user_pipe_msg, " ");
						strcat(user_pipe_msg, (*it).c_str() );
					}
					broadcast(serving_client_id, USER_PIPE_RECIEVER, user_pipe_msg);
				}
			}
			break;
		}
		//if upcomming child process is target or not.
		int target_flag = false;
		int pop_out_index = -1;
		for(int i = 0; i< pipe_vector.size(); i++){				//urrent_pipe_record.isUserPipe == false.	
			if( (pipe_vector[i].client_ID == serving_client_id)&&
			    (pipe_vector[i].get_count() == 0)&&
				(current_pipe_record.isUserPipe == false) ){
				target_flag = true;
				pop_out_index = i;
				newProcessIn = pipe_vector[i].get_read();
				pipe_reached_target = pipe_vector[i];
				//write to pipe which arrives target will be useless. it'll be closed in both child and parent process.
				close(pipe_vector[i].get_write() );
			}
			//count can not decrease here. itll influence following Pioneer part.
		}

		// if it's not target, and it's doesn't creat pipe, 
		//the process in/out/err will remain shell_in/out/err_reserved.
		// for case  "cat test.html |2 ls", in which "ls" printout in shell.

		//check if upcomming process_output has the same target with any current_pipe_record's.
		int isPioneer = true;
		if(pipe_flag == true){
			for(int i = 0; i< pipe_vector.size(); i++){
				if( (pipe_vector[i].client_ID == serving_client_id) && (current_pipe_record.get_count() == pipe_vector[i].get_count() )   ){
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
			cout << "child process creating fail" << endl;
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
			} else if(create_user_pipe_flag == true){	//sending msg to reciever
				close(current_pipe_record.get_read());
				dup2(newProcessOut, STDOUT_FILENO);
				dup2(newProcessErr, STDERR_FILENO);
				close(newProcessOut);  //newProcessOut = newProcessErr
			} else if (user_pipe_reciever_flag == true){
				close(current_pipe_record.get_write());		
				dup2(newProcessIn, STDIN_FILENO);		//recieve msg from sender.
				close(newProcessIn);
				cout << "getting message from user pipe." << endl;
			}
			//if there is no fd change,  eveytime child fork starts with shell_std_in/out/err
			//e.q. isolated cmd like ls in "cat file |2 ls number"

			//get argv.			
			const char *pargv[argv_table.size() + 1];
			convert_argv_to_consntchar(pargv, argv_table);
			execvp(pargv[0], (char **) pargv);
			if(execvp(pargv[0], (char **) pargv) == -1 ){
				fprintf(stderr,"Unknown command: [%s].\n",pargv[0]);
				//unknown_cmd = true; //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
			}
			
			exit(-1);

			
		} else if(pid >0 ){			//parent
			signal(SIGCHLD, childHandler);
			for(int i = 0; i< pipe_vector.size(); i++){
				if( (pipe_vector[i].client_ID == serving_client_id) && (pipe_vector[i].isUserPipe == false) ){			
					pipe_vector[i].count_decrease();
				}
			}
			if(file_flag == true ){  //close pipe_write_only 
				cout << "close file flow" << endl;
				close(newProcessOut);	
			}
			if (user_pipe_reciever_flag == true){
				usleep(10000); 
				cout << "close file user_pipe" << endl;
				current_pipe_record.close_pipe();	//userpipe finish it's job.
				pipe_vector.erase(current_pipe_record.user_pipe_record);
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
bool special_cmd(stringstream &sscmd){
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
		string special_symbol = ss.str();
		string user_pipe_with_pipe;
		size_t found = special_symbol.find("\'");
		bool isMessage = found!=string::npos;			//if it's message, ignore symbol |!><
		while((ss >> ws) && ( !strchr("|!><", ss.peek()) || isMessage) && (ss >> token)  )  //if front is false, ss>>token won't work
				argv.push_back(token);
		return argv;
}