#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>		
#include <cstring>		//strchr()
#include <vector>
#include <map>
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
#define INPUT_BUFFER_SIZE 15000
#define PORT_SIZE	6
#define MEMBER_JOIN 0
#define MEMBER_LEAVE 1
#define NAME_CHANGE 2
#define MEMBER_YELL 3
#define USER_PIPE_SENDER 4
#define USER_PIPE_RECIEVER 5

//===Chatting Room====
void broadcast(int, int, char*);  		
char Welcome[] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
char prompt[3] = "% ";
bool isUserExist(int);
//==================
//=======parsing======
void childHandler(int);
void convert_argv_to_consntchar(const char, vector<string> );
int parse_cmd(int, stringstream &);
bool special_cmd(int, stringstream&);
bool isChat_cmd(int, stringstream&);
bool shell_exit = false;
vector<string> retrieve_argv(stringstream&);
//==================
class Pipe_class{
	public: 
	Pipe_class(){
		isUserPipe = false;
		pfd[0] = -1;
		pfd[1] = -1;
		user_pipe_pfd[0] = -1;
		user_pipe_pfd[1] = -1;
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
		//cout <<"read/write are" << user_pipe_pfd[0] << "/" << user_pipe_pfd[1] << endl;
	}
	void close_pipe(){
		//cout << "pipe is closing" << endl;
		close(pfd[0]);
		close(pfd[1]);
	}
	void close_Upipe(){
		//cout << "user_pipe_pfd is closing" << endl;
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
			envVar.insert(pair<string,string>("PATH","bin:."));
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
			envVar.clear();
			envVar.insert(pair<string,string>("PATH","bin:."));
		};
		void change_name(const char* newname){
			memset( client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(client_name, newname, strlen(newname) );
		}
		char* get_charID(){
			sprintf(charID,"%d",client_ID);
			return charID;
		}
		void Csetenv(string key, string value){
			setenv(key.c_str(), value.c_str(), 1);
			map<string,string>::iterator it;
			it = envVar.find(key);
			if(it != envVar.end()){
				it->second = value;
			}else{
				envVar.insert(pair<string,string>(key,value));
			}
		}
		void Recallenv(){
			clearenv();
			for (std::map<string, string>::iterator it = envVar.begin(); it != envVar.end(); ++it)
				setenv(it->first.c_str(), it->second.c_str(), 1);
		}
		int client_fd;
		int client_ID;
		char charID[16];
		char client_name[CLIENT_NAME_SIZE];
		char IP[INET_ADDRSTRLEN];
		unsigned short int port;									//not sure about port size.
		int ID_from_sender;				// //for broadcasting msg.
		int ID_to_reciever;
		map<string,string> envVar;
};
vector<Pipe_class> pipe_vector;				
Client_state Client_manage_list[CLIENT_LIMIT] ;

int main(int argc, char* argv[]){
	
	setenv("PATH", "bin:.", 1); 


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
	
	//char input_buffer[INPUT_BUFFER_SIZE]= {0};
	int nbytes;
	int rv;		//don't know yet/////////////////////////////////
	struct addrinfo hints, *ai, *p;		
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;				//it's not fixed. give elastic option.
	hints.ai_socktype = SOCK_STREAM;	//stream: connect oriented.
	hints.ai_flags = AI_PASSIVE;				//don't know yet/////////////////////////////////
													//argv[1] is port setted when execute the program.   '80' usually used for http .etc.
	if( (rv = getaddrinfo(NULL, argv[1], &hints, &ai) ) != 0){			//getinfo helps to confige pre setting. three input and 1 output. if success,  return 0.
		//fprintf(stderr, "selectserver: %s !!!!!!!!\n", gai_strerror(rv));		//don't know yet/////////////////////////////////
		exit(1);				//don't know yet/////////////////////////////////
	}
	listener = socket(AF_INET, SOCK_STREAM, 0);		//AF_INET is IPV4. 
	if (listener == -1){
        cout <<"Fail to create a socket." << endl;
    }
	int opt = 1;
	setsockopt(listener, SOL_SOCKET,SO_REUSEADDR,&opt, sizeof(opt));
	setsockopt(listener, SOL_SOCKET,SO_REUSEPORT, &opt, sizeof(opt));
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
	int total_connections = 0;
	int serving_to_client_fd = -1;
	int serving_client_id = -1;
	bool wasOverListenLimit = false;
	struct timeval tv;
	tv.tv_sec = 1;
	while(true){
		rfds = master; // update rfds from master.
		while(select(fdmax+1, &rfds, NULL, NULL, &tv) <= -1) {
			perror("[error] select");
			//exit(4);
		}
		for(int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &rfds)) {
				//new connection 
				if (i == listener) {
					newfd = accept(listener, (struct sockaddr *)&clientSockInfo, &addrlen);
					/*	
					if(total_connections == CLIENT_LIMIT){	//restrict connection limit
						close(newfd);
						break;
					}
					total_connections++;
					*/
					if(newfd == -1){
						cout << "fail of createing client_fd" << endl;
					//fd create succeed.
					}else{
						serving_to_client_fd = newfd;
						send(newfd, Welcome, sizeof(Welcome)-1, 0 );
						
						for(int j= 1; j <CLIENT_LIMIT; j++){			
							if(Client_manage_list[j].client_ID  ==  -1 ){
								Client_manage_list[j].client_ID =  j;
								Client_manage_list[j].client_fd = newfd;
								inet_ntop(AF_INET, &(clientSockInfo.sin_addr), Client_manage_list[j].IP, INET_ADDRSTRLEN);
								Client_manage_list[j].port = htons(clientSockInfo.sin_port);
								broadcast(Client_manage_list[j].client_ID, MEMBER_JOIN, NULL);
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
					//get clientID
					for(int j = 1; j < CLIENT_LIMIT; j++){
						if(Client_manage_list[j].client_fd == i){
							serving_client_id = j;
						}
					}
					Client_manage_list[serving_client_id].Recallenv();
					

					char input_buffer[INPUT_BUFFER_SIZE]= {0};
					read(serving_to_client_fd, input_buffer, sizeof(input_buffer) );
					//handling '\r' from telnet.			
					char temp[INPUT_BUFFER_SIZE];
					int k =0;
					strncpy(temp, input_buffer, sizeof(input_buffer)); 
					memset( input_buffer, '\0', sizeof(input_buffer)*sizeof(char) );
					for(int i = 0; i< sizeof(temp); i++ ){
						if(temp[i] == '\r'){
							continue;
						}else if(temp[i] == '\n'){
							continue;
						}else{
							input_buffer[k] = temp[i];
							k++;
						}
					}
					
					string cmd = input_buffer;
					//connection aborted
					if( cmd == "exit"){
					    //dprintf(serving_to_client_fd,"client is  leaving \n");
						broadcast(Client_manage_list[serving_client_id].client_ID, MEMBER_LEAVE, NULL);
						//dprintf(serving_to_client_fd,"your cmd after braodcast is %s ***\n", cmd.c_str());
						for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it < pipe_vector.end(); ++it){
							if( (*it).client_ID == serving_client_id || (*it).sender == serving_client_id){	
								(*it).close_pipe();
								if((*it).isUserPipe == true )
									(*it).close_Upipe();
								pipe_vector.erase(it);
							}
						}
						Client_manage_list[serving_client_id].reset();
						clearenv();					//reset parent env
						setenv("PATH", "bin:.", 1) ;
						close(i);
						FD_CLR(i, &master);
						break;
					}else{	//there is data comming
						stringstream sscmd(cmd);
						dup2(serving_to_client_fd, STDIN_FILENO);
						dup2(serving_to_client_fd, STDOUT_FILENO);
						dup2(serving_to_client_fd, STDERR_FILENO);
						if( !special_cmd(serving_client_id, sscmd)){
							if(shell_exit == true){
								//no op
							}else if(isChat_cmd(serving_client_id,sscmd) ){
								//no op if not chat_cmd continue
							}else{
								parse_cmd(serving_client_id, sscmd);
							}
						}
					}
					dup2(SERVER_STDIN_FD, STDIN_FILENO);
					dup2(SERVER_STDOUT_FD, STDOUT_FILENO);
					dup2(SERVER_STDERR_FD, STDERR_FILENO);
				}
			usleep(10000);
			send(serving_to_client_fd, prompt, sizeof(prompt)-1, 0);
			}
		}
	}
	return 0;
}
bool isUserExist(int lookingID){
	for(int  ID_transverse= 1; ID_transverse < CLIENT_LIMIT; ID_transverse++){
		if(Client_manage_list[ID_transverse].client_ID == lookingID ){
			return true;
		}
	}
	return false;
}
void broadcast(int ID_num, int action, char* msg){
	stringstream ss;
	string broadcast_content;
	ss.str("");
	ss.clear();
	switch(action){
		case MEMBER_JOIN:
			ss <<  "*** User \'" << Client_manage_list[ID_num].client_name;
			ss << "\' entered from ";
			ss << Client_manage_list[ID_num].IP; ss << ":"; ss << Client_manage_list[ID_num].port;
			ss << ". ***\n";
			broadcast_content = ss.str();
			break;
		case MEMBER_LEAVE:
			ss << "*** User \'" << Client_manage_list[ID_num].client_name;
			ss << "\' left. ***\n" ;
			broadcast_content = ss.str();
	
			break;
		case NAME_CHANGE:
			ss << "*** User from "; ss << Client_manage_list[ID_num].IP; ss << ":"; ss << Client_manage_list[ID_num].port;
			ss << " is named \'"  ; ss << Client_manage_list[ID_num].client_name;	 ss << "\'. ***\n";
			broadcast_content = ss.str();
			break;
		case MEMBER_YELL:
			ss << "*** " << Client_manage_list[ID_num].client_name;  ss << " yelled ***:";  ss << msg; ss <<"\n";
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
	for(int j= 1; j < CLIENT_LIMIT; j++){			
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
		dprintf(Client_manage_list[serving_client_id].client_fd, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
		for(int  ID_transverse= 1; ID_transverse <CLIENT_LIMIT; ID_transverse++){
			if(Client_manage_list[ID_transverse].client_ID != -1){
				if(serving_client_id == ID_transverse ){
					dprintf(Client_manage_list[serving_client_id].client_fd, "%d\t%s\t%s:%d\t<-me\n",Client_manage_list[ID_transverse].client_ID, Client_manage_list[ID_transverse].client_name, Client_manage_list[ID_transverse].IP, Client_manage_list[ID_transverse].port);
				}else{
					dprintf(Client_manage_list[serving_client_id].client_fd, "%d\t%s\t%s:%d\n",Client_manage_list[ID_transverse].client_ID, Client_manage_list[ID_transverse].client_name, Client_manage_list[ID_transverse].IP, Client_manage_list[ID_transverse].port);
				}
			}
		}
		return true;
	}
	
	if(argv_table.at(0) == "name" ){
		//Client_manage_list[j].change_name(argv_table.at(1).c_str());
		//check if name already exist or not.
		bool check_name_existence = false;
		for(int  ID_transverse= 1; ID_transverse < CLIENT_LIMIT; ID_transverse++){
			if(strcmp(argv_table.at(1).c_str(), Client_manage_list[ID_transverse].client_name) == 0 ){
				check_name_existence = true;
			}
		}
		if(check_name_existence == true){
			dprintf(Client_manage_list[serving_client_id].client_fd, "*** User '%s' already exists. ***\n",argv_table.at(1).c_str());
		}else{
			memset(Client_manage_list[serving_client_id].client_name, '\0', CLIENT_NAME_SIZE*sizeof(char) );
			strncpy(Client_manage_list[serving_client_id].client_name, argv_table.at(1).c_str(), argv_table.at(1).length()  );
			broadcast(serving_client_id, NAME_CHANGE, Client_manage_list[serving_client_id].client_name);
		}
		return true;
	}
	
	if(argv_table.at(0) == "yell" ){
		char message[1024] = {0};

		strcat(message, " ");
		cmdline = cmdline.substr(5,cmdline.length()) ;
		strcat(message, cmdline.c_str());
		
		broadcast(Client_manage_list[serving_client_id].client_ID, MEMBER_YELL, message);
		return true;
	}				

	if(argv_table.at(0) == "tell" ){
		if(!isUserExist(   atoi( argv_table.at(1).c_str()  )      )     ){
			dprintf(Client_manage_list[serving_client_id].client_fd, "*** Error: user #%s does not exist yet. ***\n",argv_table.at(1).c_str());
			return true;
		}
		char message[1024] = "*** ";
		strcat(message, Client_manage_list[serving_client_id].client_name);
		strcat(message, " told you ***:");
		/*
		for(vector<string>::iterator it = argv_table.begin()+2; it < argv_table.end(); ++it){
			strcat(message, " ");
			strcat(message, (*it).c_str() );
			strcat(message, "\n");
		}
		*/
		strcat(message, " ");
		cmdline = cmdline.substr(7,cmdline.length()) ;
		cmdline += "\n";
		strcat(message, cmdline.c_str());
		send(Client_manage_list[atoi( argv_table.at(1).c_str() ) ].client_fd, message, strlen(message), 0);
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
	bool Upipe_err_flag;
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
		Upipe_err_flag = false;
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
			if((*peek_recieve)[0] == '<' ){
				if( isdigit( (*peek_recieve)[1]) ){
					string sign_number = (*peek_recieve).substr(1); //skip the first charactor.	
					replace(sign_number.begin(), sign_number.end(), '+', ' ');
					stringstream numb(sign_number);
					int pipenumber = 0;
					int temp;
					while (numb >> temp){
						pipenumber += temp;
					}

					//looking for user pipe.
					for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it< pipe_vector.end(); ++it){
						if( ((*it).isUserPipe == true ) && ((*it).client_ID == serving_client_id) && ((*it).sender == pipenumber) ){
							//cout << "argv_table size " << argv_table.size() << endl;
							argv_table.pop_back();
							isRecieveing_Upipe = true;
							Client_manage_list[serving_client_id].ID_from_sender = pipenumber;	//for braodcast msg.
							current_pipe_record = (*it);			//get user_pipe fd, ID, senderID, reciever ID , private pipe ID
							//cout << "read, write" << current_pipe_record.get_Upipe_read() << ", " << current_pipe_record.get_Upipe_write() << endl;
							//cout << "pipe_vector size  before get: " << pipe_vector.size() << endl;
							pipe_vector.erase(it);
							newProcessIn = current_pipe_record.get_Upipe_read();
							break;
						}
					}
					//check if Upipe accept success. otherwise user(ID)/Upipe not exist.
					if(isUserExist(pipenumber) == false){	 //if not exist, return 0;
						dprintf(Client_manage_list[serving_client_id].client_fd, "*** Error: user #%d does not exist yet. ***\n", pipenumber);
						argv_table.pop_back();
						newProcessIn = open("/dev/null", O_RDWR);	
						Upipe_err_flag = true;
					}else if(isRecieveing_Upipe == false){
						dprintf(Client_manage_list[serving_client_id].client_fd, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", Client_manage_list[pipenumber].client_ID, Client_manage_list[serving_client_id].client_ID);
						Upipe_err_flag = true;
						argv_table.pop_back();
						newProcessIn = open("/dev/null", O_RDWR);
					}else{		
						char user_pipe_msg[1024] = {0};
						string tmp = sscmd.str();
						strcpy(user_pipe_msg, tmp.c_str());
						broadcast(serving_client_id, USER_PIPE_RECIEVER, user_pipe_msg);
					}
					
				}
			}
		}
		if(sign_number[0] == '|'){
				//cout << "I am piping " << endl;
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
					current_pipe_record.set_numPipe_count(pipenumber);
				} else {
					current_pipe_record.set_numPipe_count(1);
				}
		}
		if(sign_number[0] =='!'){
				//pratically the same as mentioned ahead.
				current_pipe_record.client_ID = serving_client_id;
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
						dprintf(Client_manage_list[serving_client_id].client_fd,"*** Error: user #%d does not exist yet. ***\n",pipenumber);
						//argv_table.pop_back();
						Upipe_err_flag = true;
						newProcessIn = open("/dev/null", O_RDWR);
						newProcessOut = newProcessIn;
						//return 0;
					}
					for(vector<Pipe_class>::iterator it = pipe_vector.begin(); it< pipe_vector.end(); ++it){	//check if user_pipe exist.
						if( ((*it).isUserPipe == true ) && ((*it).client_ID == pipenumber)  && ((*it).sender == serving_client_id) ){
								dprintf(Client_manage_list[serving_client_id].client_fd,"*** Error: the pipe #%d->#%d already exists. ***\n",Client_manage_list[serving_client_id].client_ID, Client_manage_list[pipenumber].client_ID);
								//argv_table.pop_back();
								Upipe_err_flag = true;
								newProcessIn = open("/dev/null", O_RDWR);
								newProcessOut = newProcessIn;
								//return 0;
							}
					}
					//user pipe is avaliable to create.
					if(Upipe_err_flag == false){
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
						current_pipe_record.creat_Upipe();
						//cout << "read, write" << current_pipe_record.get_Upipe_read() << ", " << current_pipe_record.get_Upipe_write() << endl;
						current_pipe_record.set_numPipe_count(999);    
						pipe_vector.push_back( current_pipe_record );
						
						newProcessOut = current_pipe_record.get_Upipe_write();
						newProcessErr = current_pipe_record.get_Upipe_write();
						
						//broadcast
						char user_pipe_msg[1024] ={0};
						string tmp = sscmd.str();
						strcpy(user_pipe_msg, tmp.c_str());
						broadcast(serving_client_id, USER_PIPE_SENDER, user_pipe_msg);
					}
				}else{
					file_flag = true;
					string filename;
					sscmd >> filename;
					newProcessOut = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC , 00777);
				}
				//break;
		}
		
		//if upcomming child process is target or not.
		int target_flag = false;
		int pop_out_index = -1;				//for releasig from pipe_vector
		for(int i = 0; i< pipe_vector.size(); i++){				//there would be only on input in a instruction. won't be ls | cat <0
			//cout << "ID, isUserpipe, rfd, wfd, count " << pipe_vector[i].client_ID << ", " << pipe_vector[i].isUserPipe << ", " << endl;
			//cout << pipe_vector[i].get_read() << ", " << pipe_vector[i].get_write() << ", " << pipe_vector[i].get_count() << endl;
			if( (pipe_vector[i].client_ID == serving_client_id)&&   (pipe_vector[i].get_count() == 0) && pipe_vector[i].isUserPipe == false ){
				
				//cout << " I am target" << endl;
				target_flag = true;
				pop_out_index = i;
				if(Upipe_err_flag == false)
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
					//cout << "getting in L661 " << endl;
					
					newProcessOut = pipe_vector[i].get_write();
					isPioneer = false;
					if(shockMarckflag == true){
						newProcessErr = pipe_vector[i].get_write();
					}
					//cout << "reditection. read: " <<  newProcessIn << ", write: " << newProcessOut << endl;
					//cout << "fcurrent_pipe_record.get_read()  :  " << current_pipe_record.get_read()  << endl;
				}
			}
			if (isPioneer == true) {		//it's pioneer pipe. create it !
				current_pipe_record.creat_pipe();
				//cout << "createing pipe!!!"<< "read: " <<  current_pipe_record.get_read() << ", write: " << current_pipe_record.get_write() << endl;
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
					//cout << "fork: double_Upipe == false, close: " <<newProcessIn << current_pipe_record.get_Upipe_write() << endl;
					close(current_pipe_record.get_Upipe_write() );
				}else{
					//cout << "fork: double_Upipe == true, close: " << Upipe_write_tmp<< endl;
					close(Upipe_write_tmp);
				}
				dup2(newProcessIn, STDIN_FILENO);
				close(newProcessIn);
			}
			
			if(Upipe_err_flag == true){
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
			signal(SIGCHLD, childHandler);
			for(int i = 0; i< pipe_vector.size(); i++){
				if( (pipe_vector[i].client_ID == serving_client_id) ){			
					//cout <<"count decrease " << endl;
					pipe_vector[i].count_decrease();
				}
			}
			if(file_flag == true ){  //close pipe_write_only 
				//cout << "close file flow" << endl;
				close(newProcessOut);	
			}
			if (isRecieveing_Upipe == true ){
				usleep(10000); 
				if(double_Upipe == false){			
				//cout << "master: double_Upipe == false, release: " << current_pipe_record.get_Upipe_read() <<", "<<current_pipe_record.get_Upipe_write() <<endl;
					current_pipe_record.close_Upipe();	//userpipe finish it's job.  the origin user_pipe already erased when copy  user_pipe.to current pipe.
				}else{				// ">ID <ID" or "<ID >ID" case
					close(Upipe_read_tmp);
					close(Upipe_write_tmp);
					//cout << "master: double_Upipe == true , release: " << Upipe_read_tmp<<", " << Upipe_write_tmp<< endl;
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
			//cout << "pipe_vector size at the end: " << pipe_vector.size() << endl;
			
		}
	}
}
bool special_cmd(int serving_client_id, stringstream &sscmd){
	stringstream ss;
	string cmdline = sscmd.str();
    string parsed_word, cmd;
	
	ss << cmdline;
    ss>> parsed_word;
    if(parsed_word == "printenv"){
		ss >> parsed_word;
		char* pPath = getenv(parsed_word.c_str());
		if(pPath != NULL)
			dprintf(Client_manage_list[serving_client_id].client_fd,"%s\n",pPath);
		return true;
    }
    else if(parsed_word == "setenv"){
        ss >> cmd;
        ss >> parsed_word;
		Client_manage_list[serving_client_id].Csetenv(cmd, parsed_word);
        //setenv(cmd.c_str(), parsed_word.c_str(), 1) ;   
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
