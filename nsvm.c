#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

void usage(char *programName){
	printf(
		"Usage:"
		"\t%s <Path to directory> <Path to output vector file>\n"
		"\t%s <Path to BMP-formatted file> <Path to input vector file>"
		"\n",
		programName,
		programName
		);
}

int main(int argc, char **argv){
	if(argc != 3){
		fprintf(
			stderr,
			"This program currently takes exactly two arguments\n"
		       );
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	bool firstArgIsDir;
	struct stat statBuffer;
	if(stat(argv[1], &statBuffer) == 0){
		if(S_ISDIR(statBuffer.st_mode))
			firstArgIsDir = true;
		else if(S_ISREG(statBuffer.st_mode))
			firstArgIsDir = false;
		else{
			fprintf(
				stderr,
				"First argument is neither a file nor a "
				"directory\n"
			      );
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}else{
		perror("Error getting status of first argument");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if(stat(argv[2], &statBuffer) == 0){
		if(!S_ISREG(statBuffer.st_mode)){
			fprintf(
				stderr,
				"The second argument already exists, but is "
				"not a regular file\n"
			       );
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}else if(!firstArgIsDir){
		fprintf(
			stderr,
			"The first argument is a regular file, but the second "
			"argument doesn't exist\n"
		       );
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
