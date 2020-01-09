#include <string.h>
#include <unistd.h> 
#include <fcntl.h>
#include <stdio.h> 
#include <err.h>
#include <errno.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <stdlib.h> 
#include <netinet/in.h> 
#include <netdb.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
using namespace std;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Checks whether filename given in header is valid
bool validity_check(string filename){
	char c;
	if(filename.length() != 27) return false;
	for( u_int i = 0; i < filename.length(); i++){
		c = filename.at(i);
		if((c > 64 && c < 91) || (c > 96 && c < 123) || (c > 47 && c < 58) || c == 45 || c == 95){
			continue;
		}else{
			return false;
		}
	}
	return true;
}

//Used for get method and parse through a file and sends to client
void printing(int f, int socket ){
    string temp;
    int size;
    char c;
    while(read(f, &c, 1) != 0){
    	temp += c;
    }
    size = temp.length();
    string content = "Content-Length: " + to_string(size) + "\r\n\r\n";
    string header = "HTTP/1.1 200 OK\r\n" + content + temp;
    send(socket, header.c_str(), header.length(), 0);
}

//Parses through a Get Header and checks to make sure it is valid
void get_parse(string header, int socket){
	int first = (header.find(" ") + 1);
	int last = (header.find("HTTP/1.1")) - first - 1;
	string temp = header.substr(first, last);
	if(!validity_check(temp)){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
		string error = "HTTP/1.1 400 Bad Request\r\n" + content;
    	send(socket, error.c_str(), error.length(), 0);
	}
	
	int f = open(temp.c_str(), O_RDONLY);
	if(errno == 13){
    	string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
    	string error = "HTTP/1.1 403 Forbidden\r\n" + content;
    	send(socket, error.c_str(), error.length(), 0);
    }
	if(f == -1){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
    	string error = "HTTP/1.1 404 Not Found\r\n" + content;
    	send(socket, error.c_str(), error.length(), 0);
	}else{
    	printing(f, socket);
   	}
}

//Parses through a PUT header and checks if filename given is valid
string put_parse(string header, int socket){
	int first = (header.find(" ") + 1);
	int last = (header.find("HTTP/1.1")) - first - 1;
	string temp = header.substr(first, last);
	if(validity_check(temp)){
		return temp;
	}
	string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
	string error = "HTTP/1.1 400 Bad Request\r\n" + content;
    send(socket, error.c_str(), error.length(), 0);
	return "no";
}

//Goes through header and checks what method is being requested
int get_put_checker(string line){
	string temp = line.substr(0,3);
	if(temp == "GET"){
		return 1;
	}
	if(temp == "PUT"){
		return 2;
	}
	return 0;
}

//Checks length of file being passed in header
int catch_length(string line){
	if(line.substr(0,7).compare("Content") == 0){
		int first = (line.find(" ") + 1);
		string temp = line.substr(first);
		int size;
		size = stoi(temp);
		return size;
	}
	return -1;
}

//Goes through Methods passed from client and calls up subsequent methods
void parse_recv(int socket){
	int numbytes;
	int end_header = 0;
	int method_type = -1;
	char c;
	string temp, filename;
	int fd;
	int length = -1;
	int written = 0;
	try{
		while((numbytes = recv(socket, &c, 1, 0)) != 0){ //Goes through first line of header passed in to server
			temp = temp + c;
			if(temp.substr(temp.length() - 1) == "\n"){
				if(method_type == -1 || end_header == 1){
					method_type = get_put_checker(temp);
				}
				if(method_type == 1){// GET Method function call
					method_type = -1;
					get_parse(temp, socket);
					temp = "";
				}
				if(method_type == 2){// PUT Method function calls
					method_type = -1;
					filename = put_parse(temp, socket);
					remove(filename.c_str());
					fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0777);
					temp = "";
				}
			}
			if(temp.length() > 2 && temp.substr(temp.length() - 2) == "\r\n" && length == -1 ){// Checks for length of file
				length = catch_length(temp);
				temp = "";
			}
			if(end_header == 1 && temp.substr(temp.length() - 1) != "\0" && (written <= length || length == -1)){// For PUT method writes file to local file
				written += write(fd, &c, 1);
				if(written == length){// Send Response header
					string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
	    			string header = "HTTP/1.1 201 Created\r\n" + content;
	    			send(socket, header.c_str(), header.length(), 0);
				}
			}
			if(temp.length() > 3 && temp.substr(temp.length() - 4) == "\r\n\r\n"){ //Checks for end of header
				end_header = 1;
				temp = "";
				if(method_type == 0 && end_header != 1){ // If method requset is not in header sends 400
					//printf("%d\n", 1);
					string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
					string error = "HTTP/1.1 400 Bad Request\r\n" + content;
			    	send(socket, error.c_str(), error.length(), 0);
				}
			}
		}
	}catch(...){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
	    string header = "HTTP/1.1 500 Created\r\n" + content;
		send(socket, header.c_str(), header.length(), 0);
	}
}

int main(int argc, char * argv[]){
	struct addrinfo hints, *addrs;
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	if(argc != 3){
		warn("%d", argc);
	}
		//Assigns argv to port and host
		char * port = argv[2];
		char * hostname = argv[1];

		
		//Following codes defines server and sockets for client to connect to
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		getaddrinfo(hostname, port, &hints, &addrs);
		int main_socket = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
		int enable = 1;

		setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
		bind(main_socket, addrs->ai_addr, addrs->ai_addrlen);
		listen (main_socket, 16);
		

		try{
		addr_size = sizeof their_addr;
		int new_fd = 0;

		//Searches for any connection attempts to server and creates a socket to connect to client
		while(main_socket > 0){
			new_fd = accept(main_socket, (struct sockaddr *)&their_addr, &addr_size);
			parse_recv(new_fd);
		}


		close(new_fd);
		return 0;

	}catch(...){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
	    string header = "HTTP/1.1 500 Created\r\n" + content;
		char *char_header = new char[header.length()];
		strcpy(char_header, header.c_str());
		send(main_socket, char_header, sizeof(char_header), 0);
	}
}