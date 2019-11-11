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
char Welcome[] = "***************************************\n** Welcome to the information server. **\n***************************************\n";
char prompt[] = "% ";
bool isUserExist(int);
//==================



//=======parsing======
void childHandler(int);
void convert_argv_to_consntchar(const char, vector<string> );
int parse_cmd(int, stringstream &);
bool special_cmd(stringstream&);
bool isChat_cmd(int, stringstream&);
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
	int get_Upipe_read(){
		return user_pipe_pfd[0];
	}
	int get_Upipe_write(){
		return user_pipe_pfd[1];
	}
	void set_Upipe_raed(int read){
		user_pipe_pfd[0] = read;
	}
	void set_Upipe_write(int write){
		user_pipe_pfd[1] = write;
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
	void creat_Upipe(){
		pipe(user_pipe_pfd);
	}
	void close_pipe(){
		cout << "pipe is closing" << endl;
		close(pfd[0]);
		close(pfd[1]);
	}
	void close_Upipe(){
		cout << "user_pipe_pfd is closing" << endl;
		close(user_pipe_pfd[0]);
		close(user_pipe_pfd[1]);
	}
		int pfd[2];
		int user_pipe_pfd[2];
		int numPipe_count;
		int client_ID;		//a kind of pipe for private user use. it can also be reciever ID
		int sender;		//a pipe between user.
		bool isUserPipe;
};
class Client_state{
	public:
		void Client_state_init(){
			client_fd = -1;
			client_ID = -1;
			strncpy(client_name,"(no name)",strlen("(no name)") );
			memset( IP, 0, INET_ADDRSTRLEN*sizeof(char) );
			port = 0;
			ID_from_sender = -1;
			ID_to_reciever = -1;
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
			//memset( enviroment_variable, '\0', 512*sizeof(char) );
			//strncpy(enviroment_variable, "PATH", "bin:.", strlen(newname) );
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
		int ID_from_sender;				// //for broadcasting msg.
		int ID_to_reciever;
		//char enviroment_variable[512];
};
vector<Pipe_class> pipe_vector;				
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
								Client_manage_list[j].client_ID =  j;
								Client_manage_list[j].client_fd = newfd;

								inet_ntop(AF_INET, &(clientSockInfo.sin_addr), Client_manage_list[j].IP, INET_ADDRSTRLEN);
								Client_manage_list[j].port = clientSockInfo.sin_port;
								broadcast(Client_manage_list[j].client_ID, MEMBER_JOIN, NULL);
								cout << Client_manage_list[j].IP << " is connect" << endl;				//for server use.
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
						dup2(serving_to_client_fd, STDIN_FILENO);
						dup2(serving_to_client_fd, STDOUT_FILENO);
						dup2(serving_to_client_fd, STDERR_FILENO);
						if( !special_cmd(sscmd)){
							if(shell_exit == true){
								//no op
							}else if(isChat_cmd(serving_client_id,sscmd) ){
								//no op if not chat_cmd continue
							}else{
								parse_cmd(serving_client_id, sscmd);
							}
						}
						dup2(SERVER_STDIN_FD, STDIN_FILENO);
						dup2(SERVER_STDOUT_FD, STDOUT_FILENO);
						dup2(SERVER_STDERR_FD, STDERR_FILENO);
					}
				}
			usleep(10000);
			send(serving_to_client_fd, prompt, sizeof(prompt), 0);
			}
		}
	}
	return 0;
}
bool isUserExist(int lookingID){
	for(int  ID_transverse= 0; ID_transverse <CLIENT_LIMIT; ID_transverse++){
		if(Client_manage_list[ID_transverse].client_ID == lookingID ){
			return true;
		}
	}
	return false;
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
			ss << "** User from "; ss << Client_manage_list[ID_num].IP; ss << ":"; ss << Client_manage_list[ID_num].port;
			ss << " is named \'"  ; ss << Client_manage_list[ID_num].client_name;	 ss << "\'. ***\n";
			broadcast_content = ss.str();
			break;
		case MEMBER_YELL:
			ss << "*** " << Client_manage_list[ID_num].client_name;  ss << " yelled ***:";  ss << msg;
			broadcast_content = ss.str();
			break;
		case USER_PIPE_SENDER:
			ss << "*** "; ss << Client_manage_list[ID_num].client_name; ss << " (#"; ss << Client_manage_list[ID_num].get_charID(); ss << ") ";
			ss << "just piped \'"; ss << msg; ss << "\' to ";
			ss << Client_manage_list[Client_manage_list[ID_num].ID_to_reciever].client_name; ss << " (#"; ss << Client_manage_list[Client_manage_list[ID_num].ID_to_reciever].get_charID();
			ss << ") ***\n";
			broadcast_content = ss.str();
			break;
		case USER_PIPE_RECIEVER:
			ss << "*** "; ss << Client_manage_list[ID_num].client_name; ss << " (#"; ss << Client_manage_list[ID_num].get_charID(); ss << ") ";
			ss << "just received from "; ss << Client_manage_list[ Client_manage_list[ID_num].ID_from_sender  ].client_name; ss << " (#"; ss << Client_manage_list[ Client_manage_list[ID_num].ID_from_sender ].get_charID();
			ss << ") by \'";  ss << msg;  ss << "\' ***\n";
			broadcast_content = ss.str();
			break;
	}
	for(int j= 0; j <CLIENT_LIMIT; j++){			
		if(Client_manage_list[j].client_ID  !=  -1 ){  
			send(Client_manage_list[j].client_fd, broadcast_content.c_str(), broadcast_content.length(), 0 );
		}
	}
}
bool isChat_cmd(int serving_client_id, stringstream &sscmd){
	string cmdline = sscmd.str();
	stringstream chat_cmd;
	chat_cmd << cmdline;
	vector<string> argv_table = retrieve_argv(chat_cmd);   // parse out cmd before sign |!><
	if (argv_table.empty() ){
		return true;
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
		return true;
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
			string name_exist = "*** User \'";  name_exist += argv_table.at(1) + "\' already exists. ***";
			cout << name_exist << endl;
		}else{
			memset(Client_manage_list[serving_client_id].client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(Client_manage_list[serving_client_id].client_name, argv_table.at(1).c_str(), argv_table.at(1).length()  );
			broadcast(serving_client_id, NAME_CHANGE, Client_manage_list[serving_client_id].client_name);
		}
		return true;
	}
	
	if(argv_table.at(0) == "yell" ){
		char message[1024] = {0};
		for(vector<string>::iterator it = argv_table.begin()+1 ; it < argv_table.end(); ++it){
				strcat(message, " ");
				strcat(message, (*it).c_str() );
				}	
		broadcast(Client_manage_list[serving_client_id].client_ID, MEMBER_YELL, message);
		return true;
	}				

	if(argv_table.at(0) == "tell" ){
		/*bool isUserExist = false;
		for(int  ID_transverse= 0; ID_transverse <CLIENT_LIMIT; ID_transverse++){
			if(Client_manage_list[ID_transverse].client_ID == atoi( argv_table.at(1).c_str() ) ){
				isUserExist = true;
			}
		}*/
		if(isUserExist(   atoi( argv_table.at(1).c_str()  )      )     ){
			cout << "*** Error: user #"<< argv_table.at(1) << " does not exist yet. ***" << endl;
			return true;
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
		return true;
	}		
	return false;;
	//===============ChatRoom cmd  end================
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
//why order of convert_argv_to_consntchar and parse_cmd will cause 
// error: invalid conversion from ‘const char**’ to ‘char’ [-fpermissive]
//convert_argv_to_consntchar(pargv, argv_table);
int parse_cmd(int serving_client_id, stringstream &sscmd){
	bool pipe_flag;	
	bool create_user_pipe_flag;
	bool isRecieveing_Upipe;
	bool shockMarckflag;
	bool file_flag;
	bool target_flag;
	//bool unknown_cmd = false;     //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
	Pipe_class current_pipe_record;
	Pipe_class pipe_reached_target;

	while(!sscmd.eof() ){      			//check sstream of cmdline is not eof.
		//unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
		int newProcessIn = STDIN_FILENO;   //shell process's fd 0,1,2 are client's, who gives cmd, std in/out/err.
 		int newProcessOut = STDOUT_FILENO;
		int newProcessErr = STDERR_FILENO;
		
		pipe_flag = false;
		create_user_pipe_flag = false;
		isRecieveing_Upipe = false;
		shockMarckflag = false;
		file_flag	= false;
		target_flag = false;
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
			if((*peek_recieve)[0] == '<'){
				if( isdigit( (*peek_recieve)[1]) ){
					string sign_number = (*peek_recieve).substr(1); //skip the first charactor.	
					replace(sign_number.begin(), sign_number.end(), '+', ' ');
					stringstream numb(sign_number);
					int pipenumber = 0;
					int temp;
					while (numb >> temp){
						pipenumber += temp;
					}
					//check if user exist or not.
					if(isUserExist(pipenumber) == false){	 //if not exist, return 0;
						cout << "*** Error: user #" << pipenumber << " does not exist yet. ***" << endl;
						return 0;
					}
					//looking for user pipe.
					for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it< pipe_vector.end(); ++it){
						if( ((*it).isUserPipe == true ) && ((*it).client_ID == serving_client_id) && ((*it).sender == pipenumber) ){
							cout << "argv_table size " << argv_table.size() << endl;
							argv_table.pop_back();
							isRecieveing_Upipe = true;
							Client_manage_list[serving_client_id].ID_from_sender = pipenumber;	//for braodcast msg.
							current_pipe_record = (*it);			//get user_pipe fd, ID, senderID, reciever ID
							//cout << "read, write" << current_pipe_record.get_Upipe_read() << ", " << current_pipe_record.get_Upipe_write() << endl;
							//cout << "pipe_vector size  before get: " << pipe_vector.size() << endl;
							pipe_vector.erase(it);
							newProcessIn = current_pipe_record.get_Upipe_read();
							break;
						}
					}
					//avoid pipe not exist. if true(recieve success), then broadcast.  if false ,return 0;
					if(isRecieveing_Upipe == false){
						string err_msg = "** Error: the pipe #"; err_msg += Client_manage_list[pipenumber].get_charID();
						err_msg += "->#"; 								err_msg += Client_manage_list[serving_client_id].get_charID();
						err_msg += " does not exist yet. ***";
						cout << err_msg << endl;
						return 0;
					}
					char user_pipe_msg[1024] = {0};
					string tmp = sscmd.str();
					strcpy(user_pipe_msg, tmp.c_str());
					broadcast(serving_client_id, USER_PIPE_RECIEVER, user_pipe_msg);
					
				}
			}
		}
	

		if(sign_number[0] == '|'){
				cout << "I am piping " << endl;
				current_pipe_record.client_ID = serving_client_id;
				
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
					//char *pch;					 //version 2 which fucked me
					/*
					//version 2 which fucked me 
					char temp[sign_number.length()+1];
					strcpy(temp,sign_number.c_str());
					pch = strtok(temp,"+");
					while(pch != NULL){
						test = atoi(pch);
						pipenumber += test;
						pch = strtok(NULL,"+");
					}
					*/
					current_pipe_record.set_numPipe_count(pipenumber);
				} else {
					current_pipe_record.set_numPipe_count(1);
				}
		}
		if(sign_number[0] =='!'){
				//pratically the same as mentioned ahead.
				current_pipe_record.client_ID = serving_client_id;
				//cout << "not shock" << endl;
				pipe_flag = true;
				shockMarckflag = true;
				if( isdigit( sign_number[1]) ){			//don't pop sign, in case of '|' at the end of cmd.
					sign_number = sign_number.substr(1);				//skip the first charactor.		
					current_pipe_record.set_numPipe_count(atoi(sign_number.c_str()));
				} else {
					current_pipe_record.set_numPipe_count(1);
				}
		}
		int Upipe_read_tmp;   //L 608
		int Upipe_write_tmp;	// L609
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
						cout << "*** Error: user #"<<pipenumber << " does not exist yet. ***" << endl;
						return 0;
					}
					string err_msg;
					for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it< pipe_vector.end(); ++it){	//check if user_pipe exist.
						if( ((*it).isUserPipe == true ) && ((*it).client_ID == pipenumber)  && ((*it).sender == serving_client_id) ){
								err_msg += "*** Error: the pipe #";
								err_msg += Client_manage_list[serving_client_id].get_charID();  err_msg += "->#";  err_msg += Client_manage_list[pipenumber].get_charID();
								err_msg += " already exists. ***";
								cout << err_msg << endl;
								return 0;
							}
					}
					//user pipe is avaliable to create.
					Client_manage_list[serving_client_id].ID_to_reciever = pipenumber; //for broadcasting msg.
					create_user_pipe_flag = true;
					
					if(isRecieveing_Upipe == true){
						double_Upipe = true;
						Upipe_read_tmp = current_pipe_record.get_Upipe_read();
						Upipe_write_tmp =current_pipe_record.get_Upipe_write();
					}
					current_pipe_record.isUserPipe = true;
					current_pipe_record.client_ID = pipenumber;				// clientID  is the same as reciever.
					current_pipe_record.sender = serving_client_id;
					cout << "getting in L607 " << endl;
					current_pipe_record.creat_Upipe();
					cout << "read, write" << current_pipe_record.get_Upipe_read() << ", " << current_pipe_record.get_Upipe_write() << endl;
					current_pipe_record.set_numPipe_count(999);    
					pipe_vector.push_back( current_pipe_record );
					
					newProcessOut = current_pipe_record.get_Upipe_write();
					newProcessErr = current_pipe_record.get_Upipe_write();
					
					//broadcast
					char user_pipe_msg[1024] ={0};
					string tmp = sscmd.str();
					strcpy(user_pipe_msg, tmp.c_str());
					broadcast(serving_client_id, USER_PIPE_SENDER, user_pipe_msg);
					
				}else{
					file_flag = true;
					string filename;
					sscmd >> filename;
					newProcessOut = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC , 00777);
				}
				//break;
		}
		
		cout << "pipe_vector size : " << pipe_vector.size() << endl;
		
		//if upcomming child process is target or not.
		int target_flag = false;
		int pop_out_index = -1;				//for releasig from pipe_vector
		for(int i = 0; i< pipe_vector.size(); i++){				//there would be only on input in a instruction. won't be ls | cat <0
			cout << "ID, isUserpipe, rfd, wfd, count " << pipe_vector[i].client_ID << ", " << pipe_vector[i].isUserPipe << ", " << endl;
			cout << pipe_vector[i].get_read() << ", " << pipe_vector[i].get_write() << ", " << pipe_vector[i].get_count() << endl;
			if( (pipe_vector[i].client_ID == serving_client_id)&&   (pipe_vector[i].get_count() == 0) && pipe_vector[i].isUserPipe == false ){
				
				cout << " I am target" << endl;
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
				if( (pipe_vector[i].client_ID == serving_client_id) &&  (current_pipe_record.get_count() == pipe_vector[i].get_count() )  ){
					cout << "getting in L661 " << endl;
					
					newProcessOut = pipe_vector[i].get_write();
					isPioneer = false;
					if(shockMarckflag == true){
						newProcessErr = pipe_vector[i].get_write();
					}
					cout << "reditection. read: " <<  newProcessIn << ", write: " << newProcessOut << endl;
					cout << "fcurrent_pipe_record.get_read()  :  " << current_pipe_record.get_read()  << endl;
				}
			}
			if (isPioneer == true) {		//it's pioneer pipe. create it !
				current_pipe_record.creat_pipe();
				cout << "createing pipe!!!"<< "read: " <<  current_pipe_record.get_read() << ", write: " << current_pipe_record.get_write() << endl;
				current_pipe_record.isUserPipe = false;
				pipe_vector.push_back( current_pipe_record );
				newProcessOut = current_pipe_record.get_write();
				if(shockMarckflag == true ){
					newProcessErr = current_pipe_record.get_write();
				}
			}			
		}
		pid_t pid;
		while ( ( pid = fork() ) < 0){
			cout << "child process creating fail" << endl;
		}
		if( pid == 0){	 											// child
			
			if(pipe_flag == true){						
				cout << "fork: pipe_flag == true, close: " << current_pipe_record.get_read() << ", " << newProcessOut<< endl;
				close(current_pipe_record.get_read());			//don't know why sometimes it close STDIN_FDNO....
				dup2(newProcessOut, STDOUT_FILENO);  
				if(shockMarckflag == true ){
					dup2(newProcessErr, STDERR_FILENO);		// newProcessErr = newProcessOut
				}
				close(newProcessOut);
			} else if(file_flag == true ) {
				dup2(newProcessOut, STDOUT_FILENO);	
				close(newProcessOut);
			} else if(create_user_pipe_flag == true){	//sending msg to reciever
				close(current_pipe_record.get_Upipe_read());
				dup2(newProcessOut, STDOUT_FILENO);
				dup2(newProcessErr, STDERR_FILENO);
				close(newProcessOut);  //newProcessOut = newProcessErr
			}
			//if there is no fd change,  eveytime child fork starts with shell_std_in/out/err.  e.q. isolated cmd like ls in "cat file |2 ls number"
			if(target_flag == true){
				dup2(newProcessIn, STDIN_FILENO);  
				close(newProcessIn);
			}
			if(isRecieveing_Upipe == true){
				if( double_Upipe == false){
					cout << "fork: double_Upipe == false, close: " <<newProcessIn << current_pipe_record.get_Upipe_write() << endl;
					close(current_pipe_record.get_Upipe_write() );
				}else{
					cout << "fork: double_Upipe == true, close: " << Upipe_write_tmp<< endl;
					close(Upipe_write_tmp);
				}
				dup2(newProcessIn, STDIN_FILENO);
				close(newProcessIn);
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
			signal(SIGCHLD, childHandler);
			for(int i = 0; i< pipe_vector.size(); i++){
				if( (pipe_vector[i].client_ID == serving_client_id) ){			
					cout <<"count decrease " << endl;
					pipe_vector[i].count_decrease();
				}
			}
			if(file_flag == true ){  //close pipe_write_only 
				cout << "close file flow" << endl;
				close(newProcessOut);	
			}
			if (isRecieveing_Upipe == true ){
				usleep(10000); 
				if(double_Upipe == false){			
				cout << "master: double_Upipe == false, release: " << current_pipe_record.get_Upipe_read() <<", "<<current_pipe_record.get_Upipe_write() <<endl;
					current_pipe_record.close_Upipe();	//userpipe finish it's job.  the origin user_pipe already erased when copy  user_pipe.to current pipe.
				}else{				// ">ID <ID" or "<ID >ID" case
					close(Upipe_read_tmp);
					close(Upipe_write_tmp);
					cout << "master: double_Upipe == true , release: " << Upipe_read_tmp<<", " << Upipe_write_tmp<< endl;
				}
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
		size_t found = special_symbol.find("\'");
		bool isMessage = (found!=string::npos);			//if it's message, ignore symbol |!><
		while((ss >> ws) && ( !strchr("|!>", ss.peek()) || isMessage) && (ss >> token)  ){ 
				argv.push_back(token);
		}
		return argv;
}
