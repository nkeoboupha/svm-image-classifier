#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>


//Print usage message in case of failure
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

// Determine if the correct number of arguments are passed and
// if appropriate paths are provided
bool validArgs(
	int argc,
	char **argv
	){
	// Verify that exactly two paths are passed to the program
	if(argc != 3){
		fprintf(
			stderr,
			"This program currently takes exactly two arguments\n"
		       );
		return false;
	}

	/* 
	 * Use a bool to store whether the first path is either a file or 
	 * directory. Exit with an error if neither.
	 */
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
			return false;
		}
	}else{
		perror("Error getting status of first argument");
		return false;
	}
	

	/*
	 * Exit if something other than a file exists at the second path
	 * or if the first path is a file and the second path doesn't exit
	 */
	if(stat(argv[2], &statBuffer) == 0){
		if(!S_ISREG(statBuffer.st_mode)){
			fprintf(
				stderr,
				"The second argument already exists, but is "
				"not a regular file\n"
			       );
			return false;
		}
	}else if(!firstArgIsDir){
		fprintf(
			stderr,
			"The first argument is a regular file, but the second "
			"argument doesn't exist\n"
		       );
		return false;
	}
	return true;
}

int main(int argc, char **argv){
	if(!validArgs(argc, argv)){
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
