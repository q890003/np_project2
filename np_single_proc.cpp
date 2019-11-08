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
#define CLIENT_LIMIT 30
#define CLIENT_NAME_SIZE 32
#define PORT_SIZE	6
#define MEMBER_JOIN 0
#define MEMBER_LEAVE 1
#define MEMBER_YELL 2

//===Chatting Room====
void broadcast(int, int);  		//(int client_ID, int Action)
char Welcome[] = "***************************************\n** Welcome to the information server **\n***************************************\n";
char prompt[] = "% ";
//==================



//=======parsing======
void childHandler(int);
void convert_argv_to_consntchar(const char, vector<string> );
void parse_cmd(int, stringstream &);
bool special_cmd(stringstream&);
bool shell_exit = false;
vector<string> retrieve_argv(stringstream&);
//==================

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
		int client_ID;
};
class Client_state{
	public:
		void Client_state_init(){
			client_fd = -1;
			client_ID = -1;
			holding_service = false;
			strncpy(client_name,"(no name)",strlen("(no name)") );
		}
		void reset(){
			client_fd = -1;
			client_ID = -1;
			memset( client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(client_name,"(no name)",strlen("(no name)") );
			memset( IP, 0, INET_ADDRSTRLEN*sizeof(char) );
			port = 0;
		};
		void change_name(const char* newname){
			memset( client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(client_name, newname, strlen(newname) );
		}
		int client_fd;
		int client_ID;
		char client_name[CLIENT_NAME_SIZE];
		char IP[INET_ADDRSTRLEN];
		unsigned short int port;									//not sure about port size.
		string content_buffer;				// char 1024 for yell and tell.
		//Pipe_class user_pipe;
		
		bool holding_service;
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
								broadcast(Client_manage_list[j].client_ID, MEMBER_JOIN);
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
						broadcast(Client_manage_list[serving_client_id].client_ID, MEMBER_LEAVE);
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
			usleep(1000);
			send(serving_to_client_fd, prompt, sizeof(prompt), 0);
			}
		}
	}
	return 0;
}

void broadcast(int ID_num, int action){
	stringstream ss;
	string broadcast_content;
	switch(action){
		case MEMBER_JOIN:
			ss <<  "*** User " << Client_manage_list[ID_num].client_name;
			ss << "entered from ";
			ss << Client_manage_list[ID_num].IP;
			ss << ". *** \n";
			broadcast_content = ss.str();
			break;
		case MEMBER_LEAVE:
			ss << "*** User \'" << Client_manage_list[ID_num].client_name;
			ss << "\' left. *** \n" ;
			broadcast_content = ss.str();
			break;
		case MEMBER_YELL:
			ss << "*** " << Client_manage_list[ID_num].client_name;
			ss << " yelled ***: " ;
			ss << Client_manage_list[ID_num].content_buffer;
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
void parse_cmd(int serving_client_id, stringstream &sscmd){
	bool pipe_flag;	
	bool pipe_create_flag;
	bool shockMarckflag;
	bool file_flag;
	bool target_flag;
	bool unknown_cmd = false;     //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
	Pipe_class current_pipe_record;
	Pipe_class pipe_reached_target;

	while(!sscmd.eof() && !unknown_cmd){      			//check sstream of cmdline is not eof.
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
		
		vector<string> argv_table = retrieve_argv(sscmd);   // parse out cmd before sign |!>
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
			memset(Client_manage_list[serving_client_id].client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(Client_manage_list[serving_client_id].client_name, argv_table.at(1).c_str(), argv_table.at(1).length()  );
			break;		//not sure if there would be a bug/
		}
		
		if(argv_table.at(0) == "yell" ){
			Client_manage_list[serving_client_id].content_buffer = "";
			for(vector<string>::iterator it = argv_table.begin()+1; it < argv_table.end(); ++it){
				Client_manage_list[serving_client_id].content_buffer += " " +*it;
			}
			Client_manage_list[serving_client_id].content_buffer += "\n";
			
	
			broadcast(Client_manage_list[serving_client_id].client_ID, MEMBER_YELL);
			break;		//not sure if there would be a bug/
		}				

		if(argv_table.at(0) == "tell" ){

			Client_manage_list[serving_client_id].content_buffer = "*** ";
			Client_manage_list[serving_client_id].content_buffer += Client_manage_list[serving_client_id].client_name;
			Client_manage_list[serving_client_id].content_buffer += " told you";
			Client_manage_list[serving_client_id].content_buffer += " ***: ";
			for(vector<string>::iterator it = argv_table.begin()+2; it < argv_table.end(); ++it){
				Client_manage_list[serving_client_id].content_buffer +=  " "+ *it;
			}
			Client_manage_list[serving_client_id].content_buffer += "\n";
			
			
			send(Client_manage_list[atoi( argv_table.at(1).c_str() ) ].client_fd, 
					Client_manage_list[serving_client_id].content_buffer.c_str(),
					Client_manage_list[serving_client_id].content_buffer.length(), 0);
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
			if(  (pipe_vector[i].client_ID == serving_client_id)   &&    pipe_vector[i].get_count() == 0    ){
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
			execvp(pargv[0], (char **) pargv);
			if(execvp(pargv[0], (char **) pargv) == -1 ){
				fprintf(stderr,"Unknown command: [%s].\n",pargv[0]);
				//unknown_cmd = true; //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
			}
			
			exit(-1);

			
		} else if(pid >0 ){			//parent
			signal(SIGCHLD, childHandler);
			for(int i = 0; i< pipe_vector.size(); i++){
				if( pipe_vector[i].client_ID == serving_client_id ){			
					pipe_vector[i].count_decrease();
				}
			}
			//cout << "yo ho~~" << endl;
			if(file_flag == true){
				close(newProcessOut);
			}
			if(target_flag == true){
				//pipe_reached_target.close_pipe();
				close(pipe_reached_target.get_read());
				pipe_vector.erase(pipe_vector.begin()+pop_out_index);
			}
			/*if(STDOUT_FILENO == newProcessOut || file_flag == true ){		//command pid want to printout to console. or wait writting to file.
				int status;															
				waitpid(pid, &status, 0);
			}*/
			
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

		while((ss >> ws) && !strchr("|!>", ss.peek()) && (ss >> token)  )
				argv.push_back(token);
		return argv;
}