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
#include <fcntl.h>		//open()
#include <string.h>	//for version2 at line 135-145 strtok
#include <stdlib.h>	//for version2 at line 135-145 strtok
bool shell_exit = false;

using namespace std;
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
void childHandler(int signo){
	int status;
	while (waitpid(-1, &status, WNOHANG) > 0);
}
vector<Pipe_class> pipe_vector;
void convert_argv_to_consntchar(const char *argv[], vector<string> &argv_table) {
	for (int i=0; i<argv_table.size(); i++) {
		argv[i] = argv_table[i].c_str();
	}
	argv[argv_table.size()] = NULL;
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
		
		vector<string> argv_table = retrieve_argv(sscmd);   // parse out cmd before sign |!>
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
			execvp(pargv[0], (char **) pargv);
			if(execvp(pargv[0], (char **) pargv) == -1 ){
				fprintf(stderr,"Unknown command: [%s].\n",pargv[0]);
				unknown_cmd = true; //unknown command still not work. if "ls | cd", pipe still remain in the npshell. 
			}
			exit(-1);

			
		} else if(pid >0 ){			//parent
			signal(SIGCHLD, childHandler);
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

int main(){
	setenv("PATH", "bin:.", 1) ; 
    string cmd;
    while(true){
        cout << "% ";
        getline(cin, cmd);
		stringstream sscmd(cmd);
			if( !special_cmd(sscmd)){
				if(shell_exit == true){
					break;
				}
				parse_cmd(sscmd);
			}
    }
}
