/*
 * nsvm.c
 *
 * Copyright 2025 Nathaniel Keoboupha
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>

#define NUM_STEPS 4000000
#define STEP_REPORT_INTERVAL 100
#define LAMBDA 0.0001
// Debug = 0, Info = 1, Off >= 2
#define DEBUG_LEVEL 0


// Determine if system is little-endian
bool systemIsLittleEndian(){
	uint_least16_t testInt = 0x0001;
	char *testChar = (char*)&testInt;
	return *testChar == 1;
}
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

// Determine if the correct number of arguments are passed 
// and if appropriate paths are provided
bool validArgs(
	int argc,
	char **argv,
	bool *firstArgIsDir
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
	struct stat statBuffer;
	if(stat(argv[1], &statBuffer) == 0){
		if(S_ISDIR(statBuffer.st_mode))
			*firstArgIsDir = true;
		else if(S_ISREG(statBuffer.st_mode))
			*firstArgIsDir = false;
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
	}else if(*firstArgIsDir == false){
		fprintf(
			stderr,
			"The first argument is a regular file, but the second "
			"argument doesn't exist\n"
		       );
		return false;
	}
	return true;
}

bool hasBmpMagicNumber(char* pathToFile){
	FILE *bmpFile = fopen(pathToFile, "rb");
	if(!bmpFile){
		fprintf(
			stderr,
			"Could not open %s\n",
			pathToFile
		       );
		return false;
	}
	char *magicNum = (char *)malloc(2 * sizeof(char));
	if(fread(magicNum, 1, 2, bmpFile) != 2){
		if(feof(bmpFile))
			fprintf(
				stderr,
				"Reached end of file %s\n",
				pathToFile
			       );
		else if(ferror(bmpFile))
			fprintf(
				stderr,
				"Error reading from %s\n",
				pathToFile
			       );
		else
			fprintf(
				stderr,
				"Unknown error reading from %s\n",
				pathToFile
			       );
		free(magicNum);
		fclose(bmpFile);
		return false;
	}
	if(strncmp(magicNum, "BM", 2) != 0){
		fprintf(
			stderr,
			"First two bytes of %s do not match \"BM\"\n",
			pathToFile
		       );
		free(magicNum);
		fclose(bmpFile);
		return false;
	}
	free(magicNum);
	if(fclose(bmpFile) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToFile
		       );
		return false;
	}
	return true;
}

// Extract width, height, and bits per pixel from a BMP file at the provided
// path
// Use the width, height, and bits per pixel to assert that the expected size 
// matches the reported size
bool getBmpDims(
		char *pathToFile,
		uint32_t *width,
		int32_t *height,
		uint16_t *bitsPerPixel
	       ){
	if(!hasBmpMagicNumber(pathToFile)){
		fprintf(
			stderr,
			"Could not identify %s as a BMP file\n",
			pathToFile
		       );
		return false;
	}
	FILE *bmpFile = fopen(pathToFile, "rb");
	if(!bmpFile){
		fprintf(
			stderr,
			"Could not open %s\n",
			pathToFile
		       );
		return false;
	}

	// Get size of BMP file
	uint32_t fileSize;
	if(fseek(bmpFile, 2, SEEK_CUR) != 0){
		fprintf(
			stderr,
			"Error seeking to file size in %s\n",
			pathToFile
		       );
		fclose(bmpFile);
		return false;
	}
	if(!fread(&fileSize, 4, 1, bmpFile)){
		fprintf(
			stderr,
			"Error reading size of %s\n",
			pathToFile
		       );
		fclose(bmpFile);
		return false;
	}

	// Get size of collective headers 
	if(fseek(bmpFile, 4, SEEK_CUR) != 0){
		fprintf(
			stderr,
			"Error seeking to offset to data in %s\n",
			pathToFile
		       );
		fclose(bmpFile);
		return false;
	}
	uint32_t headersSize;
	if(!fread(&headersSize, 4, 1, bmpFile)){
		if(feof(bmpFile))
			fprintf(
				stderr,
				"Reached end of file %s\n",
				pathToFile
			       );
		else if(ferror(bmpFile))
			fprintf(
				stderr,
				"Error reading offset to data from %s\n",
				pathToFile
			       );
		else
			fprintf(
				stderr,
				"Unknown error reading from %s\n",
				pathToFile
			       );
		fclose(bmpFile);
		return false;
	}

	//Get height, width, bits per pixel
	if(fseek(bmpFile, 4, SEEK_CUR) != 0){
		fprintf(
			stderr,
			"Error reading from %s\n",
			pathToFile
		       );
		fclose(bmpFile);
		return false;
	}
	if(fread(width, 4, 1, bmpFile) == 0){
		if(feof(bmpFile))
			fprintf(
				stderr,
				"Reached end of file %s\n",
				pathToFile
			       );
		else if(ferror(bmpFile))
			fprintf(
				stderr,
				"Error reading from %s\n",
				pathToFile
			       );
		else
			fprintf(
				stderr,
				"Unknown error reading from %s\n",
				pathToFile
			       );
		fclose(bmpFile);
		return false;
	}
	if(fread(height, 4, 1, bmpFile) == 0){
		if(feof(bmpFile))
			fprintf(
				stderr,
				"Reached end of file %s\n",
				pathToFile
			       );
		else if(ferror(bmpFile))
			fprintf(
				stderr,
				"Error reading from %s\n",
				pathToFile
			       );
		else
			fprintf(
				stderr,
				"Unknown error reading from %s\n",
				pathToFile
			       );
		fclose(bmpFile);
		return false;
	}
	if(fseek(bmpFile, 2, SEEK_CUR) != 0){
		fclose(bmpFile);
		fprintf(
			stderr,
			"Error reading from %s",
			pathToFile
		       );
		return false;
	}
	if(fread(bitsPerPixel, 2, 1, bmpFile) == 0){
		if(feof(bmpFile))
			fprintf(
				stderr,
				"Reached end of file %s\n",
				pathToFile
			       );
		else if(ferror(bmpFile))
			fprintf(
				stderr,
				"Error reading from %s\n",
				pathToFile
			       );
		else
			fprintf(
				stderr,
				"Unknown error reading from %s\n",
				pathToFile
			       );
		fclose(bmpFile);
		return false;
	}
	if(fclose(bmpFile) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToFile
		       );
		return false;
	}

	// Check for legal output values
	if(*width == 0){
		fprintf(
			stderr,
			"Width is 0\n"
		       );
		return false;
	}
	if(*height == 0){
		fprintf(
			stderr,
			"Height is 0\n"
		       );
		return false;
	}
	// Valid bpp values are 1, 2, 4, 8, 16, 24, and 32 for the BMP format
	if(*bitsPerPixel != 24){
		bool isAcceptableBpp = false;
		for(
			uint8_t validBppVal = 1; 
			validBppVal <= 32; 
			validBppVal <<= 1
		){
			if(*bitsPerPixel == validBppVal){
				isAcceptableBpp = true;
				break;
			}
		}
		if(!isAcceptableBpp){
			fprintf(
				stderr,
				"%s does not have a correct bpp for the BMP "
				"format\n",
				pathToFile
			       );
			return false;
		}
	}

	if(*bitsPerPixel & 7){
		fprintf(
			stderr,
			"%s does not contain a whole number of bytes, and is "
			"thus not currently supported\n",
			pathToFile
		       );
		return false;
	}

	uintmax_t expectedSize = 
		((uintmax_t)*width + (4 - *width % 4) % 4) *
		(uintmax_t)imaxabs(*height) *
		(*bitsPerPixel >> 3) + 
		headersSize;
	if(
		expectedSize > 0xFFFFFFFF || 
		fileSize != expectedSize
	  ){
		fprintf(
			stderr,
			"Error: Expected size of %s does not match actual "
			"size. Incorrect file format or unsupported "
			"features, such as compression, likely.\n",
			pathToFile
		       );
		return false;
	}

	return true;
}

// Checks that all regular files with the BMP magic number have the same 
// dimensions
bool allFilesSameDims(
		char *pathToClassDir,
		uint32_t *width,
		int32_t *height,
		uint16_t * bitsPerPixel
		){
	struct stat direntStatus;
	struct dirent *dirEntry;
	DIR *classDir = opendir(pathToClassDir);
	if(!classDir){
		fprintf(
			stderr,
			"Error opening %s\n",
			pathToClassDir
		       );
		return false;
	}
	*width = 0;
	*height = 0;
	*bitsPerPixel = 0;
	while(dirEntry = readdir(classDir)){
		// Ignore hidden directories and 
		// anything that isn't a regular file
		if(dirEntry->d_name[0] == '.')
			continue;
		char *pathToSample = 
			(char *)
			malloc(
				strlen(pathToClassDir) +
				strlen(dirEntry->d_name) + 2
			      );
		if(!pathToSample){
			fprintf(
				stderr,
				"Error allocating memory for path to sample\n"
			       );
			closedir(classDir);
			return false;
		}
		strcpy(pathToSample, pathToClassDir);
		strcat(pathToSample, "/");
		strcat(pathToSample, dirEntry->d_name);

		if(stat(pathToSample, &direntStatus) != 0){
			fprintf(
				stderr,
				"Error getting status of %s\n",
				pathToSample
			       );
			free(pathToSample);
			closedir(classDir);
			return false;
		}

		if(
			!S_ISREG(direntStatus.st_mode) ||
			!hasBmpMagicNumber(pathToSample)
		){
			free(pathToSample);
			continue;
		}

		if(
			*height == 0 &&
			*width == 0 &&
			*bitsPerPixel == 0
		  ){
			if(
				!getBmpDims(
					pathToSample,
					width,
					height,
					bitsPerPixel
					)
			  ){
				fprintf(
					stderr,
					"Error initializing BMP dimensions "
					"using %s\n",
					pathToSample
				       );
				free(pathToSample);
				closedir(classDir);
				return false;
			}
		}else{
			uint32_t sampleWidth;
			int32_t sampleHeight;
			uint16_t sampleBitsPerPixel;
			if(
				!getBmpDims(
					pathToSample,
					&sampleWidth,
					&sampleHeight,
					&sampleBitsPerPixel
					)
			  ){
				fprintf(
					stderr,
					"Error getting dimensions of %s for "
					"comparison\n",
					pathToSample
				       );
				free(pathToSample);
				closedir(classDir);
				return false;
			}
			if(
				sampleWidth != *width ||
				imaxabs(sampleHeight) != imaxabs(*height) ||
				sampleBitsPerPixel != *bitsPerPixel
			  ){
				if(sampleWidth != *width)
					fprintf(
						stderr,
						"Width of %s does not match "
						"that of another BMP file in "
						"%s\n"
						"Expected: %d\tActual:%d\n",
						pathToSample,
						pathToClassDir,
						*width,
						sampleWidth
					       );
				if(imaxabs(sampleHeight) != imaxabs(*height))
					fprintf(
						stderr,
						"Height of %s does not match "
						"that of another BMP file in "
						"%s\n"
						"Expected: %d\tActual:%d\n",
						pathToSample,
						pathToClassDir,
						*height,
						sampleHeight
					       );
				if(sampleBitsPerPixel != *bitsPerPixel)
					fprintf(
						stderr,
						"%s has a different number of "
						"bits per pixel than that of "
						"another BMP file in %s\n",
						pathToSample,
						pathToClassDir
					       );
				free(pathToSample);
				closedir(classDir);
				return false;
			}
		}
		free(pathToSample);
	}
	if(closedir(classDir) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToClassDir
		       );
		return false;
	}
	return true;
}

bool initializeOutputFile(
		char *pathToInputDir,
		char *pathToOutputFile
		){
	if(access(pathToOutputFile, F_OK) == 0){
		if(access(pathToOutputFile, W_OK) != 0){
			fprintf(
				stderr,
				"Insufficient permission to overwrite %s\n",
				pathToOutputFile
			       );
			return false;
		}
	}

	FILE *output = fopen(pathToOutputFile, "wb");
	if(!output){
		fprintf(
			stderr,
			"Error opening %s for writing\n",
			pathToOutputFile
		       );
		return false;
	}

	// Write magic number to output file
	char *svmMagicNumber = "NSVM";
	if(fwrite(svmMagicNumber, 1, 4, output) != 4){
		fprintf(
			stderr,
			"Error writing magic number to %s\n",
			pathToOutputFile
		       );
		fclose(output);
		return false;
	}

	// Write size of double (in chars) to output file
	uint8_t doubleSize = sizeof(double);
	if(!fwrite(&doubleSize, sizeof(uint8_t), 1, output)){
		fprintf(
			stderr,
			"Error writing size of double to %s\n",
			pathToOutputFile
		       );
		fclose(output);
		return false;
	}

	uint32_t width;
	int32_t height;
	uint16_t bitsPerPixel;
	uint64_t numClasses = 0;

	struct dirent *firstLevelDirEntry;
	DIR *firstLevelDir = opendir(pathToInputDir);
	if(!firstLevelDir){
		fprintf(
			stderr,
			"Error opening directory %s\n",
			pathToInputDir
		       );
		fclose(output);
		return false;
	}
	while(firstLevelDirEntry = readdir(firstLevelDir)){
		// Disregard hidden entries and anything that isn't a directory
		if(firstLevelDirEntry->d_name[0] == '.')
			continue;
		char *pathToFirstLevelDir = 
			(char *)
			malloc(
				sizeof(char) *
				(
				strlen(pathToInputDir) +
				strlen(firstLevelDirEntry->d_name) +
				2
			      	)
			      );
		if(!pathToFirstLevelDir){
			fprintf(
				stderr,
				"Error allocating memory for path to first "
				"level directory\n"
			       );
			fclose(output);
			closedir(firstLevelDir);
			return false;
		}
		strcpy(pathToFirstLevelDir, pathToInputDir);
		strcat(pathToFirstLevelDir, "/");
		strcat(pathToFirstLevelDir, firstLevelDirEntry->d_name);

		struct stat direntStatus;
		if(stat(pathToFirstLevelDir, &direntStatus) != 0){
			fprintf(
				stderr,
				"Error getting status of %s\n",
				pathToFirstLevelDir
			       );
			free(pathToFirstLevelDir);
			fclose(output);
			closedir(firstLevelDir);
			return false;
		}
		if(!S_ISDIR(direntStatus.st_mode)){
			free(pathToFirstLevelDir);
			continue;
		}

		// Disregard directories with regular files that don't all
		// match the established dimensions
		uint32_t dirWidth;
		int32_t dirHeight;
		uint16_t dirBitsPerPixel;
		if(
			!allFilesSameDims(
				pathToFirstLevelDir,
				&dirWidth,
				&dirHeight,
				&dirBitsPerPixel
				)
		  ){
			free(pathToFirstLevelDir);
			continue;
		}else if (numClasses != 0){
			if(
				dirWidth != width ||
				dirHeight != height ||
				dirBitsPerPixel != bitsPerPixel ||
				// Currently only whole bytes per pixel
				// supported
				dirBitsPerPixel & 7
			  ){
				free(pathToFirstLevelDir);
				continue;
			}
		// Establish dimensions on first valid directory
		}else{
			width = dirWidth;
			height = dirHeight;
			bitsPerPixel = dirBitsPerPixel;
			if(
				!fwrite(
					&width, 
					sizeof(uint32_t), 
					1, 
					output
					) ||
				!fwrite(
					&height, 
					sizeof(int32_t), 
					1, 
					output
					) ||
				!fwrite(
					&bitsPerPixel, 
					sizeof(uint16_t), 
					1, 
					output
					) ||
				// Reserve space to be overwritten later
				!fwrite(
					&numClasses,
					sizeof(uint64_t),
					1,
					output
				       )
			  ){
				fprintf(
					stderr,
					"Error writing BMP dimensions to %s "
					"using files from %s\n",
					pathToOutputFile,
					pathToFirstLevelDir
				       );
				fclose(output);
				closedir(firstLevelDir);
				free(pathToFirstLevelDir);
				return false;
			}
		}

		// Write the class name preceeded by its run length to the 
		// output file and 
		// increment the number of classes
		uint8_t classNameLength = strlen(firstLevelDirEntry->d_name);
		if(
			!fwrite(
				&classNameLength,
				sizeof(uint8_t),
				1,
				output
			       ) ||
			fwrite(
				&firstLevelDirEntry->d_name,
				sizeof(char),
				classNameLength,
				output
			      ) != classNameLength
		  ){
			fprintf(
				stderr,
				"Error writing class name and run length of "
				"%s to %s\n",
				pathToFirstLevelDir,
				pathToOutputFile
			       );
			free(pathToFirstLevelDir);
			return false;
		}
		free(pathToFirstLevelDir);
		numClasses++;
	}
	if(closedir(firstLevelDir) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToInputDir
		       );
		fclose(output);
		return false;
	}
	if(numClasses < 2){
		fprintf(
			stderr,
			"Error: fewer than 2 valid class directories\n"
		       );
		fclose(output);
		return false;
	}

	// Write initial vectors to output file
	double initialDimVal = 0.0d;
	uintmax_t numPixels = (uintmax_t)width * (uintmax_t)imaxabs(height);
	uint16_t bytesPerPixel = bitsPerPixel >> 3;
	for(uintmax_t i = 1; i < numClasses; i++){
		for(uintmax_t j = i; j < numClasses; j++){
			for(
				uintmax_t pixelNum = 0;
				pixelNum < numPixels;
				pixelNum++
			){
				for(
					uint16_t byteNum = 0;
					byteNum < bytesPerPixel;
					byteNum++
				   ){
					if(
						!fwrite(
							&initialDimVal,
							sizeof(double),
							1,
							output
						       )
					  ){
						fprintf(
							stderr,
							"Error writing "
							"initial vectors to %s"
							"\n",
							pathToOutputFile
						       );
						fclose(output);
						return false;
					}
				}
			}
		}
	}

	// Write number of classes to file
	if(
		fseek(
			output,
			4 * sizeof(char) + 
			sizeof(uint8_t) +
			sizeof(uint32_t) + 
			sizeof(int32_t) +
			sizeof(uint16_t),
			SEEK_SET
		     ) != 0
	  ){
		fprintf(
			stderr,
			"Error seeking to class number field in %s\n",
			pathToOutputFile
		       );
		fclose(output);
		return false;
	}
	if(
		!fwrite(
			&numClasses,
			sizeof(uint64_t),
			1,
			output
		      )
	  ){
		fprintf(
			stderr,
			"Error writing number of classes to %s\n",
			pathToOutputFile
		       );
		fclose(output);
		return false;
	}
	
	if(fclose(output) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToOutputFile
		       );
		return false;
	}
	return true;
}

char **getClassNamesFromFile(char *pathToSvmFile, uint64_t *classCount){
	if(access(pathToSvmFile, F_OK) != 0){
		fprintf(
			stderr,
			"%s doesn't exist\n",
			pathToSvmFile
			);
		return NULL;
	}
	if(access(pathToSvmFile, R_OK) != 0){
		fprintf(
			stderr,
			"Insufficient permission to read %s\n",
			pathToSvmFile
			);
		return NULL;
	}
	FILE *svmFile = fopen(pathToSvmFile, "rb");
	if(!svmFile){
		fprintf(
			stderr,
			"Error opening %s\n",
			pathToSvmFile
		       );
		return NULL;
	}
	if(
		fseek(
			svmFile,
			4 * sizeof(char) + 
			sizeof(uint8_t) +
			sizeof(uint32_t) + 
			sizeof(int32_t) + 
			sizeof(uint16_t),
			SEEK_SET
		     ) != 0
	  ){
		fprintf(
			stderr,
			"Error seeking class count field in %s\n",
			pathToSvmFile
		       );
		fclose(svmFile);
		return NULL;
	}
	if(
		!fread(
			classCount,
			sizeof(uint64_t),
			1,
			svmFile
		      )
	  ){
		fprintf(
			stderr,
			"Error reading class count from %s\n",
			pathToSvmFile
		       );
		fclose(svmFile);
		return NULL;
	}
	char **classNames =
		(char **)
		malloc(
			sizeof(char *) *
			*classCount
		      );
	if(!classNames){
		fprintf(
			stderr,
			"Error allocating memory for class name pointers\n"
		       );
		fclose(svmFile);
		return NULL;
	}

	for(uint64_t classNum = 0; classNum < *classCount; classNum++){
		uint8_t nameRunLength;
		if(
			!fread(
				&nameRunLength,
				sizeof(uint8_t),
				1,
				svmFile
			      )
		  ){
			fprintf(
				stderr,
				"Error reading run length of class %d of %d "
				"from %s\n",
				classNum + 1,
				classCount,
				pathToSvmFile
			       );
			fclose(svmFile);
			for(
				uint64_t prevClassNum = 0;
				prevClassNum < classNum;
				prevClassNum++
			   )
				free(classNames[prevClassNum]);
			free(classNames);
			return NULL;
		}
		classNames[classNum] = 
			(char *)
			malloc(
				sizeof(char) *
				nameRunLength +
				1
			      );
		if(!classNames[classNum]){
			fprintf(
				stderr,
				"Error allocating memory for class name\n"
			       );
			fclose(svmFile);
			for(
				uint64_t prevClassNum = 0;
				prevClassNum < classNum;
				prevClassNum++
			   )
				free(classNames[prevClassNum]);
			free(classNames);
			return NULL;
		}
		if(
			fread(
				classNames[classNum],
				sizeof(char),
				nameRunLength,
				svmFile
			     ) != nameRunLength
		  ){
			fprintf(
				stderr,
				"Error reading class name from %s\n",
				pathToSvmFile
			       );
			fclose(svmFile);
			for(
				uint64_t prevClassNum = 0;
				prevClassNum < classNum;
				prevClassNum++
			   )
				free(classNames[prevClassNum]);
			free(classNames);
			return NULL;
		}
		classNames[classNum][nameRunLength] = '\0';
	}
	return classNames;
}

uintmax_t getNumSamples(char *pathToClassDir){
	DIR *classDir = opendir(pathToClassDir);
	if(!classDir){
		fprintf(
			stderr,
			"Error opening %s\n",
			pathToClassDir
		       );
		return 0;
	}
	struct dirent *sample;
	struct stat sampleStatus;

	// Count number of non-hidden regular files that have the BMP magic 
	// number
	uintmax_t numSamples = 0;
	while(sample = readdir(classDir)){
		if(sample->d_name[0] == '.')
			continue;
		char *pathToSample = 
			(char *)
			malloc(
				strlen(pathToClassDir) +
				strlen(sample->d_name) +
				2
			      );
		if(!pathToSample){
			fprintf(
				stderr,
				"Error allocating memory for path to sample "
				"in %s\n",
				pathToClassDir
			       );
			closedir(classDir);
			return 0;
		}
		strcpy(pathToSample, pathToClassDir);
		strcat(pathToSample, "/");
		strcat(pathToSample, sample->d_name);
		if(stat(pathToSample, &sampleStatus) != 0){
			fprintf(
				stderr,
				"Error getting status of %s\n",
				pathToSample
			       );
			closedir(classDir);
			free(pathToSample);
			return 0;
		}
		if(
			!S_ISREG(sampleStatus.st_mode) ||
			!hasBmpMagicNumber(pathToSample)
		  ){
			free(pathToSample);
			continue;
		}
		free(pathToSample);
		numSamples++;

	}
	return numSamples;
}

char *getPathToRandomSample(char *pathToClassDir, uintmax_t numSamples){
	DIR *classDir = opendir(pathToClassDir);
	if(!classDir){
		fprintf(
			stderr,
			"Error opening %s\n",
			pathToClassDir
		       );
		return NULL;
	}
	uintmax_t sampleNum;
	FILE *randPipe = fopen("/dev/urandom", "rb");
	if(!randPipe){
		fprintf(
			stderr,
			"Error opening /dev/urandom\n"
		       );
		closedir(classDir);
		return NULL;
	}
	if(
		!fread(
			&sampleNum,
			sizeof(uintmax_t),
			1,
			randPipe
		      )
	  ){
		fprintf(
			stderr,
			"Error reading a uintmax_t from /dev/urandom\n"
		       );
		fclose(randPipe);
		closedir(classDir);
		return NULL;
	}
	if(fclose(randPipe) != 0){
		fprintf(
			stderr,
			"Error closing /dev/urandom\n"
		       );
		closedir(classDir);
		return NULL;
	}
	sampleNum %= numSamples;
	// Ensure at least one sample is read
	// Should be fine since empty directories are not considered
	sampleNum++;
	struct dirent *sample;
	struct stat sampleStatus;
	while(sampleNum > 0){
		sample = readdir(classDir);
		if(!sample){
			fprintf(
				stderr,
				"Error reading entry from %s\n",
				pathToClassDir
			       );
			closedir(classDir);
			return NULL;
		}
		if(sample->d_name[0] == '.')
			continue;
		char *pathToSample = 
			(char *)
			malloc(
				strlen(pathToClassDir) +
				strlen(sample->d_name) +
				2
			      );
		if(!pathToSample){
			fprintf(
				stderr,
				"Error allocating space for path to sample\n"
			       );
			closedir(classDir);
			return NULL;
		}
		strcpy(pathToSample, pathToClassDir);
		strcat(pathToSample, "/");
		strcat(pathToSample, sample->d_name);

		if(stat(pathToSample, &sampleStatus) != 0){
			fprintf(
				stderr,
				"Error getting status of %s\n",
				pathToSample
			       );
			closedir(classDir);
			return NULL;
		}
		if(
			!S_ISREG(sampleStatus.st_mode) ||
			!hasBmpMagicNumber(pathToSample)
		  ){
			free(pathToSample);
			continue;
		}
		free(pathToSample);
		sampleNum--;
	}
	char *pathToRandomSample =
		(char *)
		malloc(
			strlen(pathToClassDir) +
			strlen(sample->d_name) + 
			2
		      );
	if(!pathToRandomSample){
		fprintf(
			stderr,
			"Error allocating memory for path to random sample\n"
		       );
		closedir(classDir);
		return NULL;
	}
	strcpy(pathToRandomSample, pathToClassDir);
	strcat(pathToRandomSample, "/");
	strcat(pathToRandomSample, sample->d_name);
	if(closedir(classDir) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToClassDir
		       );
		return NULL;
	}
	return pathToRandomSample;
}

double getNormDivisor(
	char *pathToSample
	){
	if(access(pathToSample, F_OK) != 0){
		fprintf(
			stderr,
			"Error accessing %s: File does not exist\n",
			pathToSample
		       );
		return -1.0;
	}
	if(access(pathToSample, R_OK) != 0){
		fprintf(
			stderr,
			"Error accessing %s for reading\n",
			pathToSample
		       );
		return -1.0;
	}
	struct stat sampleStatus;
	if(stat(pathToSample, &sampleStatus) != 0){
		fprintf(
			stderr,
			"Error getting status of %s\n",
			pathToSample
		       );
		return -1.0;
	}
	if(!S_ISREG(sampleStatus.st_mode)){
		fprintf(
			stderr,
			"%s is not a regular file\n",
			pathToSample
		       );
		return -1.0;
	}
	if(!hasBmpMagicNumber(pathToSample)){
		fprintf(
			stderr,
			"Could not identify %s as a BMP file\n",
			pathToSample
		       );
		return -1.0;
	}

	uint32_t width;
	int32_t height;
	uint16_t bitsPerPixel;

	if(
		!getBmpDims(
			pathToSample,
			&width,
			&height,
			&bitsPerPixel
			)
	  ){
		fprintf(
			stderr,
			"Could not get dimensions of %s\n",
			pathToSample
		       );
		return -1.0;
	}

	FILE *sample = fopen(pathToSample, "rb");
	if(!sample){
		fprintf(
			stderr,
			"Error opening %s for reading\n",
			pathToSample
		       );
		return -1.0;
	}
	if(fseek(sample, 10, SEEK_SET) != 0){
		fprintf(
			stderr,
			"Error seeking to offset to data in %s\n",
			pathToSample
		       );
		fclose(sample);
		return -1.0;
	}

	uint32_t offsetBytes;
	if(fread(&offsetBytes, 4, 1, sample) == 0){\
		fprintf(
			stderr,
			"Error reading offset to data from %s\n",
			pathToSample
		       );
		fclose(sample);
		return -1.0;
	}
	if(fseek(sample, offsetBytes, SEEK_SET) != 0){
		fprintf(
			stderr,
			"Error seeking to data in %s\n",
			pathToSample
		       );
		fclose(sample);
		return -1.0;
	}
	uintmax_t sumSquareByteValues = 0;
	uint64_t numRows = imaxabs(height);
	uint8_t rowPadding = (4 - width % 4) % 4;
	for(intmax_t rowNum = 0; rowNum < numRows; rowNum++){
		for(uint32_t colNum = 0; colNum < width; colNum ++){
			uint8_t pixVal;
			if(fread(&pixVal, 1, 1, sample) == 0){
				fprintf(
					stderr,
					"Could not get pixel value in %s\n",
					pathToSample
				       );
				fclose(sample);
				return -1.0;
			}
			sumSquareByteValues += (uint16_t)pixVal * pixVal;
		}
		if(fseek(sample, rowPadding, SEEK_CUR) != 0){
			fprintf(
				stderr,
				"Could not seek to next row in %s\n",
				pathToSample
			       );
			fclose(sample);
			return -1.0;
		}
	}
	if(fclose(sample) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToSample
		       );
		return -1.0;
	}
	return sqrt((double)sumSquareByteValues);
}

bool trainVectorWithSample(
		char *pathToOutputFile,
		char *pathToSample,
		uintmax_t offsetVectors,
		uintmax_t offsetToVectors,
		double normDivisor,
		double learnRate,
		bool isPositiveSample
		){

	if(DEBUG_LEVEL < 1){
		// Print information about current vector
		fprintf(
			stderr,
			"\t\tInfo: Training %s sample with %d vectors offset\n",
			isPositiveSample ? "positive" : "negative",
			offsetVectors
		       );
	}

	// Ensure read/write permissions with output file
	if(access(pathToOutputFile, F_OK) == 0){
		if(access(pathToOutputFile, R_OK) != 0){
			fprintf(
				stderr,
				"Lacking read permissions for %s\n",
				pathToOutputFile
			       );
			return false;
		}
		if(access(pathToOutputFile, W_OK) != 0){
			fprintf(
				stderr,
				"Lacking write permissions for %s\n",
				pathToOutputFile
			       );
			return false;
		}
	}else{
		fprintf(
			stderr,
			"%s does not exist\n",
			pathToOutputFile
		       );
		return false;
	}
	if(access(pathToSample, F_OK) == 0){
		if(access(pathToSample, R_OK) != 0){
			fprintf(
				stderr,
				"Insufficient permission to read %s\n",
				pathToSample
			       );
			return false;
		}
	}else{
		fprintf(
			stderr,
			"%s does not exist\n",
			pathToSample
		       );
		return false;
	}
	struct stat fileStatus;
	if(stat(pathToOutputFile, &fileStatus) != 0){
		fprintf(
			stderr,
			"Error getting status of %s\n",
			pathToOutputFile
		       );
		return false;
	}
	if(!S_ISREG(fileStatus.st_mode)){
		fprintf(
			stderr,
			"%s is not a regular file\n",
			pathToOutputFile
		       );
		return false;
	}
	if(stat(pathToSample, &fileStatus) != 0){
		fprintf(
			stderr,
			"Error getting status of %s\n",
			pathToSample
		       );
		return false;
	}
	if(!S_ISREG(fileStatus.st_mode)){
		fprintf(
			stderr,
			"%s is not a regular file\n",
			pathToSample
		       );
		return false;
	}

	uint32_t width;
	int32_t height;
	uint16_t bitsPerPixel;

	if(
		!getBmpDims(
			pathToSample,
			&width,
			&height,
			&bitsPerPixel
			)
	  ){
		fprintf(
			stderr,
			"Error getting dimensions of %s\n",
			pathToSample
		       );
		return false;
	}
	
	if(bitsPerPixel & 7){
		fprintf(
			stderr,
			"Error: only whole number of bytes per pixel "
			"supported\n"
		       );
		return false;
	}

	uint16_t bytesPerPixel = bitsPerPixel >> 3;

	FILE *output = fopen(pathToOutputFile, "rb+");
	if(!output){
		fprintf(
			stderr,
			"Error opening %s for reading and writing\n",
			pathToOutputFile
		       );
		return false;
	}
	if(fseek(output, offsetToVectors, SEEK_SET) != 0){
		fprintf(
			stderr,
			"Error seeking to vectors in %s\n",
			pathToOutputFile
		       );
		fclose(output);
		return false;
	}

	uint64_t numPixels = width * (uint64_t)imaxabs(height);
	// Seek to the appropriate vector
	for(
		uintmax_t numVectors = 0;
		numVectors < offsetVectors; 
		numVectors++
	){
		for(
			uint64_t vectorDim = 0; 
			vectorDim < numPixels;
			vectorDim++
		   ){
			for(
				uint16_t pixelByte = 0;
				pixelByte < bytesPerPixel;
				pixelByte++
			   ){
				if(
					fseek(
						output, 
						sizeof(double), 
						SEEK_CUR
					     ) != 0
				){
					fprintf(
						stderr,
						"Error seeking to start of "
						"relevant vector in %s\n",
						pathToOutputFile
					       );
					fclose(output);
					return false;
				}
			}
		}
	}

	// Seek to first pixel in sample
	FILE *sample = fopen(pathToSample, "rb");
	if(!sample){
		fprintf(
			stderr,
			"Error opening %s for reading\n",
			pathToSample
		       );
		fclose(output);
		return false;
	}
	if(fseek(sample, 10, SEEK_SET) != 0){
		fprintf(
			stderr,
			"Error seeking to offset to data in %s\n",
			pathToSample
		       );
		fclose(sample);
		fclose(output);
		return false;
	}
	uint32_t offsetToSampleData;
	if(!fread(&offsetToSampleData, sizeof(uint32_t), 1, sample)){
		fprintf(
			stderr,
			"Error reading offset to data from %s\n",
			pathToSample
		       );
		fclose(sample);
		fclose(output);
		return false;
	}
	if(fseek(sample, offsetToSampleData, SEEK_SET) != 0){
		fprintf(
			stderr,
			"Error seeking to data in %s\n",
			pathToSample
		       );
		fclose(sample);
		fclose(output);
		return false;
	}

	// Seek to top-left pixel
	uint8_t rowPadding = (4 - width % 4) % 4;
	uint64_t numRows = (uint64_t)imaxabs(height);
	if(height > 0){
		if(fseek(sample, -(width + rowPadding), SEEK_END) != 0){
			fprintf(
				stderr,
				"Error seeking to top-left pixel in "
				"%s\n",
				pathToSample
			       );
			fclose(sample);
			fclose(output);
			return false;
		}
	}
	double dotProduct = 0.0;

	for(uint64_t rowNum = 0; rowNum < numRows; rowNum++){
		for(uint32_t colNum = 0; colNum < width; colNum++){
			for(
				uint16_t pixelByte = 0;
				pixelByte < bytesPerPixel;
				pixelByte++
			   ){
				double currentVectorDim;
				if(
					!fread(
						&currentVectorDim,
						sizeof(double),
						1,
						output
					      )
				  ){
					fprintf(
						stderr,
						"Error reading double from "
						"%s\n",
						pathToOutputFile
					       );
					fclose(output);
					fclose(sample);
					return false;
				}

				uint8_t byteValue;
				if(
					!fread(
						&byteValue,
						1,
						1,
						sample
					      )
				  ){
					fprintf(
						stderr,
						"Error reading byte from %s\n",
						pathToSample
					       );
					fclose(output);
					fclose(sample);
					return false;
				}
				dotProduct += 
					currentVectorDim *
					byteValue /
					normDivisor;
			}
		}
		if(height > 0){
			if(
				rowNum != numRows - 1 &&
				fseek(
					sample, 
					-(2 * width + rowPadding), 
					SEEK_CUR
				      ) != 0
				){
				fprintf(
					stderr,
					"Error seeking to next row in %s\n",
					pathToSample
				       );
				fclose(output);
				fclose(sample);
				return false;
			}
		}else{
			if(
				fseek(
					sample,
					rowPadding,
					SEEK_CUR
				     ) != 0
			  ){
				fprintf(
					stderr,
					"Error seeking to next row in %s\n",
					pathToSample
				       );
				fclose(output);
				fclose(sample);
				return false;
			}
		}
	}

	// Return back to beginning vector and sample
	if(height > 0){
		if(fseek(sample, -(width + rowPadding), SEEK_END) != 0){
			fprintf(
				stderr,
				"Error seeking to top-left pixel in "
				"%s\n",
				pathToSample
			       );
			fclose(sample);
			fclose(output);
			return false;
		}
	}else{
		if(fseek(sample, offsetToSampleData, SEEK_SET) != 0){
			fprintf(
				stderr,
				"Error returning to start of %s\n",
				pathToSample
			       );
			fclose(sample);
			fclose(output);
			return false;
		}
	}
	for(
		uint64_t vectorDim = 0; 
		vectorDim < numPixels;
		vectorDim++
	   ){
		for(
			uint16_t pixelByte = 0;
			pixelByte < bytesPerPixel;
			pixelByte++
		   ){
			if(
				fseek(
					output, 
					-sizeof(double), 
					SEEK_CUR
				     ) != 0
			){
				fprintf(
					stderr,
					"Error returning to start of vector "
					"in %s\n",
					pathToOutputFile
				       );
				fclose(output);
				fclose(sample);
				return false;
			}
		}
	}

	if(!isPositiveSample)
		dotProduct = -dotProduct;

	if(dotProduct < 1.0){
		if(DEBUG_LEVEL < 1){
			fprintf(
				stderr,
				"\t\t\tDot Product = %lf: Redirecting Vector\n",
				isPositiveSample? dotProduct : -dotProduct
			       );
		}
		for(uint64_t rowNum = 0; rowNum < numRows; rowNum++){
			for(uint32_t colNum = 0; colNum < width; colNum++){
				for(
					uint16_t pixelByte = 0;
					pixelByte < bytesPerPixel;
					pixelByte++
				   ){
					double vectorDim;
					if(
						!fread(
							&vectorDim,
							sizeof(double),
							1,
							output
						     )
					  ){
						fprintf(
							stderr,
							"Error reading double "
							"from %s\n",
							pathToOutputFile
						       );
						fclose(output);
						fclose(sample);
						return false;
					}
					uint8_t byteValue;
					if(
						!fread(
							&byteValue,
							1,
							1,
							sample
						     )
					  ){
						fprintf(
							stderr,
							"Error reading byte "
							"from %s\n",
							pathToSample
						       );
						fclose(output);
						fclose(sample);
						return false;
					}
					vectorDim -=
						learnRate *
						(
						(LAMBDA * vectorDim) - 
						(isPositiveSample ?
						(byteValue / normDivisor) :
						-(byteValue / normDivisor)
						)
						);
					if(
						fseek(
							output,
							-sizeof(double),
							SEEK_CUR
						     ) != 0 ||
						!fwrite(
							&vectorDim,
							sizeof(double),
							1,
							output
						      )
					  ){
						fprintf(
							stderr,
							"Error overwriting "
							"double in %s\n",
							pathToOutputFile
						       );
						fclose(output);
						fclose(sample);
						return false;
					}

				}
			}
			if(height > 0){
				if(
					rowNum != numRows - 1 &&
					fseek(
						sample, 
						-1 *
						2 * 
						width - 
						rowPadding, 
						SEEK_CUR
					      ) != 0
					){
					fprintf(
						stderr,
						"Error seeking to "
						"next row in %s\n",
						pathToSample
					       );
					fclose(output);
					fclose(sample);
					return false;
				}
			}else{
				if(
					fseek(
						sample,
						rowPadding,
						SEEK_CUR
					     ) != 0
				  ){
					fprintf(
						stderr,
						"Error seeking to "
						"next row in %s\n",
						pathToSample
					       );
					fclose(output);
					fclose(sample);
					return false;
				}
			}
		}
	}else{
		if(DEBUG_LEVEL < 1){
			fprintf(
				stderr,
				"\t\t\tDot Product = %lf: Shrinking Vector\n",
				isPositiveSample ? dotProduct : -dotProduct
			       );
		}
		for(uint64_t rowNum = 0; rowNum < numRows; rowNum++){
			for(uint32_t colNum = 0; colNum < width; colNum++){
				for(
					uint16_t pixelByte = 0;
					pixelByte < bytesPerPixel;
					pixelByte++
				   ){
					double vectorDim;
					if(
						!fread(
							&vectorDim,
							sizeof(double),
							1,
							output
						     )
					  ){
						fprintf(
							stderr,
							"Error reading double "
							"from %s\n",
							pathToOutputFile
						       );
						fclose(output);
						fclose(sample);
						return false;
					}
					vectorDim -=
						learnRate *
						LAMBDA * 
						vectorDim;
					if(
						fseek(
							output,
							-sizeof(double),
							SEEK_CUR
						     ) != 0 ||
						!fwrite(
							&vectorDim,
							sizeof(double),
							1,
							output
						      )
					  ){
						fprintf(
							stderr,
							"Error overwriting "
							"double in %s\n",
							pathToOutputFile
						       );
						fclose(output);
						fclose(sample);
						return false;
					}

				}
			}
		}
	}
	if(fclose(sample) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToSample
		       );
		fclose(output);
		return false;
	}
	if(fclose(output) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToOutputFile
		       );
		return false;
	}
	return true;
}

bool trainVectorsWithSample(
		char *pathToOutputFile,
		char *pathToSample,
		uintmax_t offsetToVectors,
		uint64_t classNum,
		uint64_t numClasses,
		double learnRate
		){
	
	//Get required information from sample
	double normDivisor = getNormDivisor(pathToSample);

	if(DEBUG_LEVEL < 1){
		fprintf(
			stderr,
			"\tDebug: Sample Magnitude = %lf\n",
			normDivisor
		       );
	}

	if(normDivisor <= 0){
		// Exit upon error
		if(normDivisor < 0){
			fprintf(
				stderr,
				"Error obtaining the norm divisor for %s\n",
				pathToSample
			       );
			return false;
		}

		// Vectors remain unchanged if all bytes equal 0
		return true;
	}

	uintmax_t offsetVectors = 0;
	// Point offset to first negative vector and train
	if(classNum != 0){
		offsetVectors = classNum - 1;
		if(
			!trainVectorWithSample(
				pathToOutputFile,
				pathToSample,
				offsetVectors,
				offsetToVectors,
				normDivisor,
				learnRate,
				false
				)
		  ){
			fprintf(
				stderr,
				"Error training sample\n"
			       );
			return false;
		}
	}

	// Train the rest of the negative vectors
	for(uintmax_t iterNum = 1; iterNum < classNum; iterNum++){
		if(offsetVectors > offsetVectors + numClasses - 1 - iterNum){
			fprintf(
				stderr,
				"Overflow occured during vector offset "
				"calculation \n"
			       );
			return false;
		}
		offsetVectors += numClasses - 1 - iterNum;
		if(
			!trainVectorWithSample(
				pathToOutputFile,
				pathToSample,
				offsetVectors,
				offsetToVectors,
				normDivisor,
				learnRate,
				false
				)
		  ){
			fprintf(
				stderr,
				"Error training sample\n"
			       );
			return false;
		}
	}

	// Find offset to first positive vector
	if(classNum != 0){
		if(offsetVectors > offsetVectors + (numClasses - classNum)){
			fprintf(
				stderr,
				"Overflow occured seeking to first positive "
				"vector\n"
			       );
			return false;
		}
		offsetVectors += numClasses - classNum;
	}

	// Train positive vectors
	for(
		uintmax_t posVectorNum = 0; 
		posVectorNum < numClasses - 1 - classNum; 
		posVectorNum++){
		if(
			!trainVectorWithSample(
				pathToOutputFile,
				pathToSample,
				offsetVectors,
				offsetToVectors,
				normDivisor,
				learnRate,
				true
				)
		  ){
			fprintf(
				stderr,
				"Error training positive sample\n"
			       );
			return false;
		}
		offsetVectors++;
	}
	return true;
}

// Function to clean up class stored class names
void freeClassNames(char **classNames, uint64_t numClasses){
	for(uint64_t classNum = 0; classNum < numClasses; classNum++){
		if(classNames[classNum] != NULL){
			free(classNames[classNum]);
			classNames[classNum] = NULL;
		}
	}
	free(classNames);
	classNames = NULL;
}

// Use the contents of the directory to make the output SVM file
bool createSvmFromDir(
		char *pathToInputDir,
		char *pathToOutputFile
		){


	// Initialize output file with metadata and vectors of magnitude 0
	if(!initializeOutputFile(pathToInputDir, pathToOutputFile)){
		fprintf(
			stderr,
			"Error writing initial output file %s\n",
			pathToOutputFile
		       );
		return false;
	}

	// Get class names from initialized file
	uint64_t numClasses;
	char **classNames = 
		getClassNamesFromFile
		(
			pathToOutputFile, 
			&numClasses
		);
	if(!classNames){
		fprintf(
			stderr,
			"Error reading classes from initialized output file "
			"%s\n",
			pathToOutputFile
		       );
		return false;
	}

	if(DEBUG_LEVEL < 1){
		for(uint64_t classNum = 0; classNum < numClasses; classNum++){
			fprintf(
				stderr,
				"Class Number %d: %s\n",
				classNum,
				classNames[classNum]
			       );
		}
	}

	// Get number of samples in each directory
	uintmax_t *filecounts = 
		(uintmax_t *)
		malloc(numClasses * sizeof(uintmax_t));
	if(!filecounts){
		fprintf(
			stderr,
			"Error allocating memory to hold number of samples in "
			"each class\n"
		       );
		freeClassNames(classNames, numClasses);
		return false;
	}
	for(uint64_t classNum = 0; classNum < numClasses; classNum++){
		char *pathToClassDir = 
			(char *)
			malloc(
				strlen(pathToInputDir) +
				strlen(classNames[classNum]) +
				2
			      );
		if(!pathToClassDir){
			fprintf(
				stderr,
				"Error allocating memory for path to "
				"class directory\n"
			       );
			freeClassNames(classNames, numClasses);
			free(filecounts);
			free(pathToClassDir);
			return false;
		}

		strcpy(pathToClassDir, pathToInputDir);
		strcat(pathToClassDir, "/");
		strcat(pathToClassDir, classNames[classNum]);

		filecounts[classNum] = getNumSamples(pathToClassDir);
		if(!filecounts[classNum]){
			fprintf(
				stderr,
				"Error getting number of samples in %s: "
				"Unable to read or directory is empty\n",
				classNum
			       );
			free(filecounts);
			free(pathToClassDir);
			freeClassNames(classNames, numClasses);
			return false;
		}
		free(pathToClassDir);
	}

	// Passing offset to avoid expensive syscalls
	uintmax_t offsetToVectors = 
		4 * sizeof(char) +
		sizeof(uint8_t) +
		sizeof(uint32_t) + 
		sizeof(int32_t) + 
		sizeof(uint16_t) + 
		sizeof(uint64_t) +
		numClasses;
	for(uintmax_t classNum = 0; classNum < numClasses; classNum++){
		offsetToVectors += strlen(classNames[classNum]);
	}

	// Commence training
	if(DEBUG_LEVEL < 2){
		fprintf(
			stderr,
			"Info: Beginning training with %d classes\n",
			numClasses
		       );
	}
	
	for(intmax_t stepNum = 0; stepNum < NUM_STEPS; stepNum++){
		//Set variable training parameters
		//double learnRate = pow(1 + stepNum, -1);
		double learnRate = 1.0 / sqrt(stepNum + 1);

		if(DEBUG_LEVEL < 2){
			if(
				DEBUG_LEVEL < 1 || 
				stepNum % STEP_REPORT_INTERVAL == 0
			){
				fprintf(
					stderr,
					"Info: Step %d of %d in progress\n",
					stepNum,
					NUM_STEPS
				       );
			}
		}
		if(DEBUG_LEVEL < 1){
			fprintf(
				stderr,
				"\tDebug: learn rate = %lf\n",
				learnRate
			       );
		}
		for(uint64_t classNum = 0; classNum < numClasses; classNum++){
			if(DEBUG_LEVEL < 1){
				fprintf(
					stderr,
					"\tDebug: Class %d of %d in progress\n",
					classNum,
					numClasses
				       );
			}

			// Select a random sample for the class and train all
			// relevant vectors

			char *pathToClassDir = 
				(char *)
				malloc(
					strlen(pathToInputDir) +
					strlen(classNames[classNum]) +
					2
				      );
			if(!pathToClassDir){
				fprintf(
					stderr,
					"Error allocating memory for path to "
					"directory of class %s\n",
					classNames[classNum]
				       );
				free(filecounts);
				freeClassNames(classNames, numClasses);
				return false;
			}

			strcpy(pathToClassDir, pathToInputDir);
			strcat(pathToClassDir, "/");
			strcat(pathToClassDir, classNames[classNum]);
			char* pathToRandomSample =
				getPathToRandomSample(
					pathToClassDir,
					filecounts[classNum]
					);
			if(!pathToRandomSample){
				fprintf(
					stderr,
					"Error getting path to random sample "
					"for class %s\n",
					classNames[classNum]
				       );
				freeClassNames(classNames, numClasses);
				free(filecounts);
				free(pathToClassDir);
				return false;
			}
			free(pathToClassDir);

			if(DEBUG_LEVEL < 1){
				fprintf(
					stderr,
					"\tDebug: Using %s for training\n",
					pathToRandomSample
				       );
			}
			if(
				!trainVectorsWithSample(
					pathToOutputFile,
					pathToRandomSample,
					offsetToVectors,
					classNum,
					numClasses,
					learnRate
					)
			  ){
				fprintf(
					stderr,
					"Error using %s for training\n",
					pathToRandomSample
				       );
				free(filecounts);
				free(pathToRandomSample);
				freeClassNames(classNames, numClasses);
				return false;
			}
			free(pathToRandomSample);
		}
	}
	free(filecounts);
	freeClassNames(classNames, numClasses);
	return true;
}

// Classify a file using a premade SVM file
bool classifyFileFromSvm(
		char *pathToInputFile,
		char *pathToSvmFile
		){

	// Get dot product between vector and normalized sample, given two open
	// files with read permissions
	//
	// Reset to start of sample, but assume that we're pointing to the 
	// correct vector
	bool tryGetDotProduct(
		FILE *svmFile,
		FILE *sampleFile,
		double normDivisor,
		uint64_t numRows,
		uint32_t offsetToData,
		uint32_t width,
		uint16_t bytesPerPixel,
		uint8_t rowPadding,
		bool heightIsPositive,
		double *dotProduct
		){
		
		// Seek to top-left pixel in sample
		if(!heightIsPositive){
			if(fseek(sampleFile, offsetToData, SEEK_SET) != 0){
				fprintf(
					stderr,
					"Error seeking to top-left "
					"pixel in %s\n",
					pathToInputFile
				       );
				return false;
			}
		}else{
			if(
				fseek(
					sampleFile,
					-(width + rowPadding),
					SEEK_END) != 0
			){
				fprintf(
					stderr,
					"Error seeking to top-left "
					"pixel in %s\n",
					pathToInputFile
				       );
				return false;
			}
		}

		// Find dot product
		*dotProduct = 0.0;
		if(normDivisor == 0.0 || normDivisor == -0.0)
			return true;

		for(uint64_t rowNum = 0; rowNum < numRows; rowNum++){
			for(
				uint32_t colNum = 0;
				colNum < width;
				colNum++
			   ){
				for(
					uint16_t byteNum = 0;
					byteNum < bytesPerPixel;
					byteNum++
				   ){
					double vectorDim;
					if(
						!fread(
							&vectorDim,
							sizeof(double),
							1,
							svmFile
						      )
					  ){
						fprintf(
							stderr,
							"Error reading "
							"support vector from "
							"file\n"
						       );
						return false;
					}
					uint8_t sampleByteValue;
					if(
						!fread(
							&sampleByteValue,
							sizeof(uint8_t),
							1,
							sampleFile
						      )
					  ){
						fprintf(
							stderr,
							"Error reading byte "
							"from sample file\n"
						       );
						return false;
					}
					*dotProduct += 
						vectorDim * 
						sampleByteValue / 
						normDivisor;
				}
			}

			// Seek to next row if not evaluated last row
			if(rowNum != numRows - 1){
				if(!heightIsPositive){
					if(
						fseek(
							sampleFile,
							rowPadding,
							SEEK_CUR
						) != 0
					  ){
						fprintf(
							stderr,
							"Error seeking past "
							"row padding in "
							"sample\n"
						       );
						return false;
					}
				}else{
					if(
						fseek(
							sampleFile,
							-1 *
							2 *
							(intmax_t)width -
							rowPadding,
							SEEK_CUR
						) != 0
					  ){
						fprintf(
							stderr,
							"Error seeking to "
							"next row in sample\n"
						       );
						return false;
					}
				}
			}

		}
		return true;
	}
	// Check that the paths exist and that we have read permissions
	if(access(pathToInputFile, F_OK) == 0){
		if(access(pathToInputFile, R_OK) != 0){
			fprintf(
				stderr,
				"Insufficient permission to read %s\n",
				pathToInputFile
			       );
			return false;
		}
	}else{
		fprintf(
			stderr,
			"%s does not exist\n",
			pathToInputFile
		       );
		return false;
	}
	if(access(pathToInputFile, F_OK) == 0){
		if(access(pathToInputFile, R_OK) != 0){
			fprintf(
				stderr,
				"Insufficient permission to read %s\n",
				pathToSvmFile
			       );
			return false;
		}
	}else{
		fprintf(
			stderr,
			"%s does not exist\n",
			pathToSvmFile
		       );
		return false;
	}

	// Check that both paths point to regular files
	struct stat fileStatus;
	if(stat(pathToInputFile, &fileStatus) != 0){
		fprintf(
			stderr,
			"Error getting status of %s\n",
			pathToInputFile
		       );
		return false;
	}
	if(!S_ISREG(fileStatus.st_mode)){
		fprintf(
			stderr,
			"%s is not a regular file",
			pathToInputFile
		       );
		return false;
	}
	if(stat(pathToSvmFile, &fileStatus) != 0){
		fprintf(
			stderr,
			"Error getting status of %s\n",
			pathToSvmFile
		       );
		return false;
	}
	if(!S_ISREG(fileStatus.st_mode)){
		fprintf(
			stderr,
			"%s is not a regular file",
			pathToSvmFile
		       );
		return false;
	}

	// Verify input file has the correct magic number and get dimensions
	if(!hasBmpMagicNumber(
		pathToInputFile
		)){
		fprintf(
			stderr,
			"Could not identify %s as a BMP file\n",
			pathToInputFile
		       );
		return false;
	}
	uint32_t width;
	int32_t height;
	uint16_t bitsPerPixel;
	if(!getBmpDims(
			pathToInputFile,
			&width,
			&height,
			&bitsPerPixel
		      )
	  ){
		fprintf(
			stderr,
			"Error obtaining BMP dimensions from %s\n",
			pathToInputFile
		       );
		return false;
	}

	// Verify that SVM file has the appropriate metadata
	// Verify that Input and SVM files have matching dimensions
	FILE *svm = fopen(pathToSvmFile, "rb");
	if(!svm){
		fprintf(
			stderr,
			"Error opening %s for reading\n",
			pathToSvmFile
		       );
		return false;
	}
	char *svmMagicNumber = (char *)malloc(sizeof(char) * 4);
	if(fread(svmMagicNumber, 1, 4, svm) != 4){
		fprintf(
			stderr,
			"Error reading magic number from %s\n",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	if(strncmp(svmMagicNumber, "NSVM", 4) != 0){
		fprintf(
			stderr,
			"%s does not have the expected magic number",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	uint8_t doubleSize;
	if(!fread(&doubleSize, 1, 1, svm)){
		fprintf(
			stderr,
			"Error reading training size of double from %s\n",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	if(doubleSize != sizeof(double)){
		fprintf(
			stderr,
			"Error: %s was trained on a machine that defines "
			"a double with a size of %d chars. This machine uses "
			"%d chars.\n",
			pathToSvmFile,
			doubleSize,
			sizeof(double)
		       );
		fclose(svm);
		return false;
	}
	uint32_t svmWidth;
	if(!fread(&svmWidth, sizeof(uint32_t), 1, svm)){
		fprintf(
			stderr,
			"Error reading training width from %s\n",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	if(svmWidth != width){
		fprintf(
			stderr,
			"%s was trained on files with a width of %d pixels. "
			"%s has a width of %d pixels.",
			pathToSvmFile,
			svmWidth,
			pathToInputFile,
			width
		       );
		fclose(svm);
		return false;
	}
	int32_t svmHeight;
	if(!fread(&svmHeight, sizeof(int32_t), 1, svm)){
		fprintf(
			stderr,
			"Error reading training height from %s\n",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	if(imaxabs(svmHeight) != imaxabs(height)){
		fprintf(
			stderr,
			"%s was trained on files with a height of %d pixels. "
			"%s has a height of %d pixels.",
			pathToSvmFile,
			svmHeight,
			pathToInputFile,
			height	
		       );
		fclose(svm);
		return false;
	}
	uint16_t svmBitsPerPixel;
	if(!fread(&svmBitsPerPixel, sizeof(uint16_t), 1, svm)){
		fprintf(
			stderr,
			"Error reading training bits per pixel from %s\n",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	if(svmBitsPerPixel != bitsPerPixel){
		fprintf(
			stderr,
			"%s was trained on files with a %d bits per pixel. %s "
			"has %d bits per pixel.\n",
			pathToSvmFile,
			svmBitsPerPixel,
			pathToInputFile,
			bitsPerPixel
		       );
		fclose(svm);
		return false;
	}
	uint64_t numClasses;
	char **classNames = getClassNamesFromFile(pathToSvmFile, &numClasses);
	if(!classNames){
		fprintf(
			stderr,
			"Error obtaining class names from %s\n",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	if(numClasses < 2){
		fprintf(
			stderr,
			"%s is improperly formatted. %s reports being trained "
			"on %d classes, while at least 2 are required",
			pathToSvmFile,
			pathToSvmFile,
			numClasses
		       );
		fclose(svm);
		return false;
	}

	// Seek to first vector
	if(fseek(svm, sizeof(uint64_t), SEEK_CUR) != 0){
		fprintf(
			stderr,
			"Error seeking past number of classes in %s\n",
			pathToSvmFile
		       );
		fclose(svm);
		return false;
	}
	for(uint64_t classNum = 0; classNum < numClasses; classNum++){
		uint8_t classRunLength;
		if(!fread(&classRunLength, 1, 1, svm)){
			fprintf(
				stderr,
				"Error reading run length of class from %s\n",
				pathToSvmFile
			       );
			fclose(svm);
			return false;
		}
		if(fseek(svm, classRunLength, SEEK_CUR) != 0){
			fprintf(
				stderr,
				"Error seeking past class name in %s\n",
				pathToSvmFile
			       );
			fclose(svm);
			return false;
		}
	}

	// Use support vectors to determine class
	uintmax_t *vectorsInFavor = 
		(uintmax_t *)malloc(numClasses * sizeof(uintmax_t));
	for(uintmax_t classNum = 0; classNum < numClasses; classNum++){
		vectorsInFavor[classNum] = 0;
	}
	if(!vectorsInFavor){
		fprintf(
			stderr,
			"Error allocating memory required for voting "
			"mechanism\n"
		       );
		fclose(svm);
		return false;
	}

	// Open open sample and find offset to bitmap data
	FILE *sample = fopen(pathToInputFile, "rb");
	if(!sample){
		fprintf(
			stderr,
			"Error opening %s for reading\n",
			pathToInputFile
		       );
		fclose(svm);
		return false;
	}
	if(fseek(sample, 10, SEEK_SET) != 0){
		fprintf(
			stderr,
			"Error seeking to offset to data in %s\n",
			pathToInputFile
		       );
		fclose(svm);
		fclose(sample);
		return false;
	}
	uint32_t offsetToData;
	if(!fread(&offsetToData, sizeof(uint32_t), 1, sample)){
		fprintf(
			stderr,
			"Error reading offset to data from %s\n",
			pathToInputFile
		       );
		fclose(svm);
		fclose(sample);
		return false;
	}

	// Get relevant values for the sample
	double normDivisor = getNormDivisor(pathToInputFile);
	if(normDivisor < 0.0){
		fprintf(
			stderr,
			"Error getting norm divisor from %s\n",
			pathToInputFile
		       );
		fclose(svm);
		fclose(sample);
		return false;
	}

	uintmax_t totalVectors = 0;
	uint64_t numRows = (uint64_t)imaxabs(height);
	uint16_t bytesPerPixel = bitsPerPixel >> 3;
	uint8_t rowPadding = (4 - width % 4) % 4;
	bool heightIsPositive = height > 0;

	for(uint64_t posClass = 0; posClass < numClasses - 1; posClass++){
		for(
			uint64_t negClass = posClass + 1;
			negClass < numClasses;
			negClass++
		   ){

			double dotProduct; 
			if(
				!tryGetDotProduct(
					svm,
					sample,
					normDivisor,
					numRows,
					offsetToData,
					width,
					bytesPerPixel,
					rowPadding,
					heightIsPositive,
					&dotProduct
					)
			  ){
				fprintf(
					stderr,
					"Error getting dot product between %s "
					"and vector in %s\n",
					pathToInputFile,
					pathToSvmFile
				       );
				fclose(svm);
				fclose(sample);
				return false;
			}
			if(dotProduct > 0.0)
				vectorsInFavor[posClass]++;
			else
				vectorsInFavor[negClass]++;
			totalVectors++;
			if(DEBUG_LEVEL < 1){
				fprintf(
					stderr,
					"Vector %d:\n"
					"\tDot Product = %lf\n"
					"\tClass = %s\n",
					totalVectors,
					dotProduct,
					classNames
						[
						dotProduct > 0.0 ? 
						posClass : 
						negClass
						]
				       );
			}
		}
	}
	if(fclose(svm) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToSvmFile
		       );
		fclose(sample);
		return false;
	}
	if(fclose(sample) != 0){
		fprintf(
			stderr,
			"Error closing %s\n",
			pathToInputFile
		       );
		return false;
	}

	// Find out and display results
	uint64_t numClassesFavorite = 0;
	uint64_t numVectorsFavor = vectorsInFavor[0];
	uint64_t *favoriteClasses = 
		(uint64_t *)malloc(numClasses * sizeof(uint64_t));
	for(uint64_t classNum = 0; classNum < numClasses; classNum++){
		if(vectorsInFavor[classNum] > numVectorsFavor){
			numClassesFavorite = 1;
			numVectorsFavor = vectorsInFavor[classNum];
			favoriteClasses[0] = classNum;
		}else if(vectorsInFavor[classNum] == numVectorsFavor){
			favoriteClasses[numClassesFavorite] = classNum;
			numClassesFavorite++;
		}
	}
	uintmax_t totalVectorsFavor = numClassesFavorite * numVectorsFavor;
	uintmax_t totalVectorsRelevant = 
		(uintmax_t)numClassesFavorite * 
		(numClasses - 1);
	fprintf(
		stdout,
		"%lf%% (%d of %d) of relevant vectors point to %s belonging "
		"to one of the following classes:\n",
		(double)totalVectorsFavor / totalVectorsRelevant * 100,
		totalVectorsFavor,
		totalVectorsRelevant,
		pathToInputFile
	       );
	for(
		uint64_t favVectorNum = 0; 
		favVectorNum < numClassesFavorite; 
		favVectorNum++
	){
		fprintf(
			stdout,
			"\t%s\n",
			classNames[favoriteClasses[favVectorNum]]
		       );
	}
	free(favoriteClasses);
	freeClassNames(classNames, numClasses);
	return true;
}

int main(int argc, char **argv){
	if(CHAR_BIT != 8){
		fprintf(
			stderr,
			"Error: System does not use 8-bit bytes"
		       );
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if(!systemIsLittleEndian()){
		fprintf(
			stderr,
			"Error: System is not litte-endian"
		       );
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	bool firstArgIsDir;
	if(!validArgs(argc, argv, &firstArgIsDir)){
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if(firstArgIsDir){
		if(!createSvmFromDir(argv[1], argv[2])){
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
		fprintf(
			stdout,
			"Training successful\n"
		       );
	}else{
		if(!classifyFileFromSvm(argv[1], argv[2])){
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	
	exit(EXIT_SUCCESS);
}
