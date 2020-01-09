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
#include <pthread.h>
#include <semaphore.h>
using namespace std;

bool l = false;
//int log_file_num = 0;
int num_args = 4;
int out = 0;
int in = 0;
int current_index = 0; 
char* log_filename;
sem_t empty, full;
int *buff;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;


//Function that generates log file
void logger_creation(string type, string body, int length, string filename, int check){
	//int index_write = 0;
	int num_of_hex_lines = length/20 + 1;
	string hex_line = "";
	int line_num = 0;
	string log = "";
	string body_temp = body;
	//printf("%d %d\n", check, length);
	if(check != -1 && length == 0){ // Used for when request was an error
        log = "FAIL: " + type + " " + filename + " HTTP/1.1 --- response " + to_string(check) + "\n";
	}
    if(check == -1 && length > 0) {// Used for when request was formatted correctly
        log = type + " " + filename + " length " + to_string(length) + "\n";
        char leading[8];
        char hex[4];
        for (int i = 0; i < num_of_hex_lines; i++) {
            line_num = 20 * i;
            sprintf(leading, "%08d", line_num);
            log = log + leading;
            for (int x = 0; x < 20; x++) {
                if (body_temp.length() == 0) {
                    break;
                }
                //int index = body_temp.at(0);
                sprintf(hex, "%02x", body_temp.at(0));
                log = log + " " + hex;
                body_temp.erase(0, 1);
            }
            log = log + "\n";
        }
        //printf("%s\n", log.c_str());
    }

	log = log + "========\n";
	//printf("%s", log.c_str());

	// Allows for multiple threads to write to the same file
	int log_file_num = open(log_filename, O_WRONLY | O_CREAT, 0777);
	pwrite(log_file_num, log.c_str(), log.length(), current_index);

	pthread_mutex_lock(&mutex_log);

	current_index += log.length();

	pthread_mutex_unlock(&mutex_log);

	close(log_file_num);
	log = "";
}


// Checks if Filename is the proper size
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

// Checks if string contains length of file
int catch_length(string line){
	int temp;
	string temp_string;
	if((temp = line.find("Content")) >= 0){
		temp_string = line.substr(temp);
		int first = (temp_string.find("Content") + 16);// Used to get filesize
		int last = (temp_string.find("\r\n")) - first;
		string ftemp = temp_string.substr(first,last);
		int size;
		size = stoi(ftemp);
		return size;
	}
	return -1;
}


// prints out http error response codes
void error_print(int err, int socket){
	if(err == 400){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
		string error = "HTTP/1.1 400 Bad Request\r\n" + content;
    	send(socket, error.c_str(), error.length(), 0);
	}
	if(err == 403){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
    	string error = "HTTP/1.1 403 Forbidden\r\n" + content;
    	send(socket, error.c_str(), error.length(), 0);
	}
	if(err == 404){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
    	string error = "HTTP/1.1 404 Not Found\r\n" + content;
    	send(socket, error.c_str(), error.length(), 0);
	}
}

//Used for get method and parse through a file and sends to client
void printing(string head, int f, int socket ){
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
    if(l){
    	string request_type = "GET";
    	logger_creation(request_type, temp, size, head, -1);
    }
}

//Used to execute Get request
void get_parse(string header, int socket){
	int first = (header.find("GET ") + 4);// Used to get Filename
	int last = (header.find("HTTP/1.1")) - first - 1;
	string temp = header.substr(first, last);
	string request_type = "GET";
	if(temp.length() == 28 && temp.at(0) == '/'){// Checks for / at start of filename and ignores
		temp = temp.substr(1, 27);
	}
	if(!validity_check(temp)){// If filename is invalid send 400 error
		error_print(400, socket);
        if(l) {
            logger_creation(request_type, "", 0, temp, 400);
        }
	}	
	int f = open(temp.c_str(), O_RDONLY);
	if(errno == 13){
    	error_print(403, socket);
        if(l) {
            logger_creation(request_type, "", 0, temp, 403);
        }
    }
	if(f == -1){
		error_print(404, socket);
        if(l) {
            logger_creation(request_type, "", 0, temp, 404);
        }
	}else{
    	printing(temp, f, socket); // Calls file to start sending client data
   	}
}

// Catches filename when request is PUT
string catch_put_filename(string header){
	int first = (header.find("PUT ") + 4); // gets the filename
	int last = (header.find("HTTP/1.1")) - first - 1;
	string temp = header.substr(first, last);
	if(temp.length() == 28 && temp.at(0) == '/'){
		temp = temp.substr(1, 27);
	}
	if(validity_check(temp)){
		return temp;
	}
	return "no";
}

//All this function is get the filename and starts the put process
void put_write(int f, int socket, char c, int* written, int *length, int* method_type, int* end_header , int* put_phase, string* body){
	*written += write(f, &c, 1);
	if(l){
		*body = *body + c;
	}
	if (*written == *length){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
		string header = "HTTP/1.1 201 Created\r\n" + content;
		send(socket, header.c_str(), header.length(), 0);
		*method_type = -1;
		*put_phase = 0;
		*end_header = 0;
		*length = -1;
	}
}

//This a function that parses each line header to check for method request type
int get_put_checker(string line){
	int temp_typeG = (line.find("GET"));
	int temp_typeP = (line.find("PUT"));
	if(temp_typeG != -1 && (line.substr(temp_typeG, 3) == "GET")){
		return 1;
	}
	if(temp_typeP != -1 && (line.substr(temp_typeP, 3) == "PUT")){
		return 2;
	}
	return 0;
}


// This function parses through all the data sent to the server
void *parse_recv(void *){
	int numbytes;
	int end_header = 0;
	int method_type = -1;
	char c;
	string body;
	string temp;
	string filename = "no";
	int fd;
	int length = -1;
	int written = 0;
	int socket;
	int put_phase = 0;
	
	// This is the start of the thread code
	while(1){
		//Consumer code
		sem_wait(&full);
		pthread_mutex_lock(&mutex1);
		socket = buff[out];
		out = (out + 1) % num_args;
		pthread_mutex_unlock(&mutex1);
		sem_post(&empty);
        string request_type = "PUT";

		// Start of Consumer consume code
		try{
			while((numbytes = recv(socket, &c, 1, 0)) != 0){ //Goes through first line of header passed in to server
				//printf("%c", c);
				if(end_header != 1 || l){
					temp = temp + c;
				}
				if(end_header == 0 && temp.length() > 3 && temp.substr(temp.length() - 4) == "\r\n\r\n"){ //Checks for end of header
					end_header = 1;
				}
				if(end_header == 1 && method_type == -1){
					method_type = get_put_checker(temp);
				}
				if(method_type == 1 && end_header == 1){// GET Method function call
					method_type = -1;
					end_header = 0;
					//printf("%s\n", temp.c_str());
					get_parse(temp, socket);
					temp = "";
				}
				if(method_type == 2 && end_header == 1){// PUT Method function calls//
					if(put_phase == 1){
						put_write(fd, socket, c, &written, &length, &method_type, &end_header , &put_phase, &body);
						//printf("%d %d\n",written, length );
						if(l && length == -1){
							logger_creation(request_type, body, written, filename, -1);
							body = "";
						}
						//printf("%d %d\n",written , length);
					}
					if(put_phase == 0 && method_type == 2){
						filename = catch_put_filename(temp);
						length = catch_length(temp);
						//printf("%s\n", filename.c_str());
						if(filename == "no" || length == -1){
							method_type = 0;
						}
						else{
							remove(filename.c_str());
							fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0777);
							put_phase = 1;
						}
						temp = "";
					}
				}
				if(method_type == 0 && end_header == 1){
					error_print(400, socket);
					if(l) {
                        logger_creation(request_type, "", 0, filename, 400);
                    }
					end_header = 0;
					method_type = -1;
				}

				
			}	
		}catch(...){
			string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
		    string header = "HTTP/1.1 500 Created\r\n" + content;
			send(socket, header.c_str(), header.length(), 0);
		}
	}
}

int main(int argc, char * argv[]){
	struct addrinfo hints, *addrs;
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	// if(argc != 3){
	// 	warn("%d", argc);
	// }	

	char opt;
	while((opt = getopt(argc, argv, "N:l:")) != -1){
		switch(opt){
			case 'N':
				num_args = stoi(optarg);
				break;
			case 'l':
				log_filename = optarg;
				//printf("%s\n", optarg);
				l = true;
				break;
			case '?':
				break;
		}
	}

	argc -= optind;
	argv += optind;



	//Assigns argv to port and host
	char * port = argv[1];
	char * hostname = argv[0];

	//printf("%s %s\n",hostname, port);

	sem_init(&empty, 0, num_args);
	sem_init(&full, 0, 0);


	// size_t n = num_args;
	buff = (int*) malloc(sizeof(int)*(num_args*800));
	
	
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
		
		for(int i = 0; i < num_args; i++){
			pthread_t tidsi;
			pthread_create(&tidsi, NULL, parse_recv, NULL);
		}
		// for(int i = 0; i < num_args; i++){
		// 	pthread_join(tids[i], NULL);
		// }

		//Searches for any connection attempts to server and creates a socket to connect to client
		while(main_socket > 0){
			new_fd = accept(main_socket, (struct sockaddr *)&their_addr, &addr_size);
			//parse_recv(new_fd);
			if(new_fd > 0){
				//printf("%d\n", new_fd);
				sem_wait(&empty);
				pthread_mutex_lock(&mutex1);
				buff[in] = new_fd;
				in = (in + 1) % num_args;
				pthread_mutex_unlock(&mutex1);
				sem_post(&full);	
			}
		}
		close(new_fd);
		free(buff);
		return 0;
	}catch(...){
		string content = "Content-Length: " + to_string(0) + "\r\n\r\n";
	    string header = "HTTP/1.1 500 Created\r\n" + content;
		char *char_header = new char[header.length()];
		strcpy(char_header, header.c_str());
		send(main_socket, char_header, sizeof(char_header), 0);
	}
}