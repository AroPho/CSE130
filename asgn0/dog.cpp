#include <stdio.h>
#include <unistd.h>
#include<fcntl.h>
#include <string.h>
#include <err.h>

// This function prints out the data parsed from the files or Stdin
void printing(int f, char buf[] ){
    int n;
    while((n = read(f, buf, 1)) > 0){
    	write(1, buf, n);
    }
}

int main(int argc, char *argv[]) {
    char buf[1]; //Array that holds data of files or stdin
    int f; // number used for open() which is int of file
    if (argc < 1){
        fprintf(stdout, "No File names indicated");
        return -1;

    }
    // If function is called without any arguments take inputs from stdin
    if(argc == 1){
        printing(0,buf);
    }
	// Go through all inputs in command line argument
    for (int i = 1; i < argc; i++){
	char* str = argv[i]; //makes arguments in strings
        //checks whether arg is a dash so inputs can be taken from stdin
	    if(strcmp(str, "-") == 0){
	        printing(0, buf);
        }
	    // parses through files and sends data to print function
        else {
            f = open(argv[i], O_RDONLY);
            if(f == -1){
                warn("%s", argv[i]);
            }
	    printing(f, buf);
        }
    }
}
