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

bool isBmpFile(
	char* pathToFile,
	uint32_t *width,
	int32_t *height,
	uint16_t *bitsPerPixel
	){
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
	if(fread(magicNum, 1, 2, bmpFile) == 0){
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
		return false;
	}
	if(strncmp(magicNum, "BM", 2) != 0){
		fprintf(
			stderr,
			"First two bytes of %s do not match \"BM\"\n",
			pathToFile
		       );
		free(magicNum);
		return false;
	}
	free(magicNum);
	if(fseek(bmpFile, 16, SEEK_CUR) != 0){
		fprintf(
			stderr,
			"Error reading from %s",
			pathToFile
		       );
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
		return false;
	}
	if(fseek(bmpFile, 2, SEEK_CUR) != 0){
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
		return false;
	}
	fclose(bmpFile);
	return true;
}

// Use the contents of the directory to make the output SVM file
bool createSvmFromDir(
		char *pathToInputDir,
		char *pathToOutputFile
		){
	struct dirent *firstLevelDirEntry;
	struct stat dirEntryStatus;
	if(access(pathToInputDir, R_OK) != 0){
		fprintf(
			stderr,
			"Insufficient permission to read %s\n",
			pathToInputDir
		       );
		return false;
	}
	if(
		access(pathToOutputFile, F_OK) == 0 &&
		access(pathToOutputFile, R_OK | W_OK)
	  ){
		fprintf(
			stderr,
			"Insufficient permission to overwrite %s\n",
			pathToOutputFile
		       );
		return false;
	}
	DIR *firstLevelDir = opendir(pathToInputDir);
	if(firstLevelDir == NULL){
		fprintf(
			stderr,
			"Could not open the provided directory"
		       );
		return false;
	}
	FILE *filecounts = tmpfile();
	if(filecounts == NULL){
		fprintf(
			stderr,
			"Could not create a temporary file"
		       );
		return false;
	}
	FILE *output;
	uintmax_t numClasses = 0;
	uint32_t width = 0;
	int32_t height = 0;
	uint16_t bitsPerPixel = 0;
	firstLevelDirEntry = readdir(firstLevelDir);
	while(firstLevelDirEntry){
		// Skip over hidden directory entries
		if(firstLevelDirEntry->d_name[0] == '.'){
			firstLevelDirEntry = readdir(firstLevelDir);
			continue;
		}
		char *pathToClassDir =
			(char *)
			malloc(
				sizeof(char) *
				(
				strlen(pathToInputDir) +
				strlen(firstLevelDirEntry->d_name) +
				2
				)
			      );
		strcpy(pathToClassDir, pathToInputDir);
		strcat(pathToClassDir, "/");
		strcat(pathToClassDir, firstLevelDirEntry->d_name);
		
		struct dirent *secondLevelDirEntry;
		DIR *secondLevelDir = opendir(pathToClassDir);
		
		uint64_t numSamples = 0;
		uint32_t sampleWidth;
		int32_t sampleHeight;
		uint16_t sampleBpp;
		secondLevelDirEntry = readdir(secondLevelDir);
		while(secondLevelDirEntry){
			if(secondLevelDirEntry->d_name[0] == '.'){
				secondLevelDirEntry = readdir(secondLevelDir);
				continue;
			}

			// Get path to sample
			char *pathToSample = 
				(char *)
				malloc(
					sizeof(char) *
					(
					strlen(pathToClassDir) +
					strlen(secondLevelDirEntry->d_name) +
					2
					)
				      );
			strcpy(pathToSample, pathToClassDir);
			strcat(pathToSample, "/");
			strcat(pathToSample, secondLevelDirEntry->d_name);

			if(
				isBmpFile(
					pathToSample,
					&sampleWidth,
					&sampleHeight,
					&sampleBpp
					)
			  ){
				// Get values from first file
				if(numClasses == 0 && numSamples == 0){
					width = sampleWidth;
					height = sampleHeight;
					bitsPerPixel = sampleBpp;
				// Compare values from all other files to the 
				// first one
				}else if(
					width != sampleWidth ||
					height != sampleHeight ||
					bitsPerPixel != sampleBpp
					){
					fprintf(
						stderr,
						"Not all BMP files in %s have "
						"the same height, width, and "
						"bits per pixel\n",
						pathToInputDir
					       );
					free(pathToSample);
					free(pathToClassDir);
					closedir(secondLevelDir);
					closedir(firstLevelDir);
					return false;
				}
			}else{
				fprintf(
					stderr,
					"Could not identify %s as a BMP file",
					pathToSample
				       );
				free(pathToSample);
				free(pathToClassDir);
				closedir(secondLevelDir);
				closedir(firstLevelDir);
				return false;
			}
			printf("\r");
			free(pathToSample);
			numSamples++;
			secondLevelDirEntry = readdir(secondLevelDir);
		}
		closedir(secondLevelDir);
		if(numSamples == 0){
			fprintf(
				stderr,
				"%s is an empty directory\n",
				pathToClassDir
			       );
			free(pathToClassDir);
			return false;
		}
		free(pathToClassDir);

		fwrite(&numSamples, sizeof(uint64_t), 1, filecounts);

		// Write the constant(?) length portion of the header 
		if(numClasses == 0){
			output = fopen(pathToOutputFile, "wb");
			char *magicNum = "NSVM";
			uint8_t doubleSize = sizeof(double);
			uint64_t numClasses = 0;
			fwrite(magicNum, strlen(magicNum), 1, output);
			fwrite(&doubleSize, sizeof(uint8_t), 1, output);
			fwrite(&width, sizeof(uint32_t), 1, output);
			fwrite(&height, sizeof(int32_t), 1, output);
			fwrite(&bitsPerPixel, sizeof(uint16_t), 1, output);
			fwrite(&numClasses, sizeof(uint64_t), 1, output);
			fclose(output);
		}
		// Write run length and name of each class
		output = fopen(pathToOutputFile, "ab");
		uint8_t classNameSize = strlen(firstLevelDirEntry->d_name);
		fwrite(&classNameSize, sizeof(uint8_t), 1, output);
		fwrite(&firstLevelDirEntry->d_name, classNameSize, 1, output);
		fclose(output);

		numClasses++;
		firstLevelDirEntry = readdir(firstLevelDir);
	}
	closedir(firstLevelDir);
	if(numClasses == 0){
		fprintf(
			stderr,
			"%s is empty\n",
			pathToInputDir
		       );
		return false;
	}else if (numClasses == 1){
		fprintf(
			stderr,
			"Multiple classes required to create SVM\n"
		       );
		return false;
	}

	if(bitsPerPixel & 7){
		fprintf(
			stderr,
			"Inputs need to have a whole number of bytes per "
			"pixel\n"
		       );
		return false;
	}
	uint_least8_t bytesPerPixel = bitsPerPixel >> 3;

	// Write number of classes to output file;
	output = fopen(pathToOutputFile, "rb+");
	fseek(
		output,
		(4 * sizeof(char)) +
		sizeof(uint8_t) +
		sizeof(uint32_t) + 
		sizeof(int32_t) +
		sizeof(uint16_t),
		SEEK_CUR
	     );
	fwrite(&numClasses, sizeof(uint64_t), 1, output);
	fclose(output);

	// Initialize support vectors to 0
	uintmax_t numVectors = 0;
	for(uintmax_t i = 1; i < numClasses; i++){
		if(numVectors + i < numVectors){
			fprintf(
				stderr,
				"Overflow occured while attempting to find "
				"the required number of vectors"
			);
			return false;
		}
		numVectors += i;
	}
	output = fopen(pathToOutputFile, "ab");
	double initialValue = 0.0;
	for(
		uint64_t i = 
			numVectors *
			width *
			(height > 0 ? height : -height) *
			bytesPerPixel;
		i > 0;
		i--
	   ){
		fwrite(&initialValue, sizeof(double), 1, output);
	}
	fclose(output);

	//Begin training
	double lambda = 1.0d;
	uintmax_t numSteps = 1800;
	int64_t bmpRowSizeBytes = 
		(width * bytesPerPixel) + ((width * bytesPerPixel) % 4);

	uintmax_t headerSize = 
		(4 * sizeof(char)) +
		sizeof(uint8_t) +
		sizeof(uint32_t) + 
		sizeof(int32_t) +
		sizeof(uint16_t) +
		sizeof(uint64_t);
	
	//Find where the support vectors start
	uintmax_t vectorStart = headerSize;
	output = fopen(pathToOutputFile, "rb");
	fseek(output, sizeof(char) * headerSize, SEEK_SET);
	for(int i = 0; i < numClasses; i++){
		vectorStart += sizeof(uint8_t);
		uint8_t classNameSize;
		fread(&classNameSize, sizeof(uint8_t), 1, output);
		fseek(output, sizeof(uint8_t) * classNameSize, SEEK_CUR);
		vectorStart += sizeof(uint8_t) * classNameSize;
	}
	fclose(output);

	intmax_t vectorSize = 
		(uintmax_t)sizeof(double) *
		width * 
		imaxabs(height) * 
		bytesPerPixel;

	for(uintmax_t stepNum = 0; stepNum < numSteps; stepNum++){
		fprintf(
			stderr,
			"Processing step %d of %d",
			stepNum + 1,
			numSteps
		       );
		// Set variable learning parameters
		double learnRate = pow((1 + stepNum), -1);

		for(uint64_t classNum = 0; classNum < numClasses; classNum++){

			// Find the path to a random sample
			output = fopen(pathToOutputFile, "rb");
			fseek(output, headerSize, SEEK_SET);
			for(
				uint64_t prevClassNum = 0; 
				prevClassNum < classNum; 
				prevClassNum++
			){
				uint8_t classNameSize;
				fread(
					&classNameSize, 
					sizeof(uint8_t), 
					1, 
					output
				);
				fseek(output, classNameSize, SEEK_CUR);
			}
			uint8_t classNameLength;
			fread(&classNameLength, sizeof(uint8_t), 1, output);
			char *pathToClassDir = 
				(char *)
				malloc(
					sizeof(char) * 
					(
					 strlen(pathToInputDir) +
					 classNameLength +
					 2
					)
				      );
			strcpy(pathToClassDir, pathToInputDir);
			strcat(pathToClassDir, "/");
			fread(
				pathToClassDir +
				(sizeof(char) * strlen(pathToClassDir)), 
				sizeof(char), 
				classNameLength,
				output
			     );
			pathToClassDir[
				strlen(pathToInputDir) + 
				classNameLength +
				1
			] = '\0';
			fclose(output);

			// Get number of samples in current class
			fseek(
				filecounts, 
				classNum * sizeof(uint64_t), 
				SEEK_SET
			);
			uint64_t numSamples;
			fread(&numSamples, sizeof(uint64_t), 1, filecounts);

			uint64_t sampleNum;
			FILE *randPipe = fopen("/dev/urandom", "rb");
			fread(&sampleNum, sizeof(uint64_t), 1, randPipe);
			fclose(randPipe);
			sampleNum %= numSamples;

			DIR *classDir = opendir(pathToClassDir);
			struct dirent *classDirEntry;
			for(
				uint64_t prevSampleNum = 0; 
				prevSampleNum < sampleNum; 
				prevSampleNum++
			){
				classDirEntry = readdir(classDir);
				if(!classDirEntry){
					fprintf(
						stderr,
						"Error reading directory "
						"entry %d of %d from %s\n",
						sampleNum,
						numSamples,
						pathToClassDir
					       );
					return false;
				}else if(classDirEntry->d_name[0] == '.'){
					prevSampleNum--;
					continue;
				}
			}
			closedir(classDir);

			if(!classDirEntry){
				fprintf(
					stderr,
					"Unable to read entry %d of %d from "
					"%s\n",
					sampleNum,
					numSamples,
					pathToClassDir
				       );
				return false;
			}

			char *pathToSample = 
				(char *)
				malloc(
					sizeof(char *) *
					(
					 strlen(pathToClassDir) +
					 strlen(classDirEntry->d_name) +
					 2
					)
				      );
			strcpy(pathToSample, pathToClassDir);
			strcat(pathToSample, "/");
			strcat(pathToSample, classDirEntry->d_name);

			FILE *sample = fopen(pathToSample, "rb");
			uint32_t offsetToBitmap;
			fseek(sample, sizeof(char) * 10, SEEK_SET);
			fread(
				&offsetToBitmap,
				sizeof(uint32_t),
				1,
				sample
			     );

			// Get sum of all pixel values for normalization
			fseek(
				sample, 
				sizeof(uint8_t) * offsetToBitmap, 
				SEEK_SET
			);
			uint64_t sumColorValues = 0;
			for(
				uint64_t rowNum = 0;
				rowNum < imaxabs(height);
				rowNum++
			   ){
				for(
					uint64_t pixelNum = 0;
					pixelNum < width * bytesPerPixel;
					pixelNum++
				   ){
					uint8_t colorVal;
					fread(
						&colorVal, 
						sizeof(uint8_t), 
						1, 
						sample
					);
					sumColorValues += colorVal;
				}
				fseek(
					sample,
					sizeof(uint8_t) * 
					(
					 bmpRowSizeBytes - 
					 (width * bytesPerPixel)
					),
					SEEK_CUR
				     );
			}
			

			// Use pseudo-randomly selected BMP to train all
			// relevant support vectors
			uintmax_t numVectsPos = numClasses - classNum - 1;
			uintmax_t numVectsNeg = classNum;

			for(
				uintmax_t vectNum = 0;
				vectNum < numClasses - 1;
				vectNum++
			   ){
				// Open SVM file to where current vector starts
				output = fopen(pathToOutputFile, "rb+");
				fseek(output, vectorStart, SEEK_SET);
				// Seek to first negative vector
				if(classNum != 0){
					for(
						uintmax_t skipVects = 0;
						skipVects < classNum - 1;
						skipVects++
					){
						fseek(
							output,
							vectorSize,
							SEEK_CUR
						     );
					}
				}
				if(vectNum < numVectsNeg){
					// Seek to current negative vector
					for(
						uintmax_t prevVectNum = 0;
						prevVectNum < vectNum;
						prevVectNum++
					   ){
						for(
							uintmax_t skipVects = 0;
							skipVects < 
							numClasses -
							2 -
							prevVectNum;
							skipVects++
						   ){
							fseek(
								output,
								vectorSize,
								SEEK_CUR
							     );
						}
					}
				}else{
					// Seek past all negative vectors
					// to first positive vector
					for(
						uintmax_t prevVectNum = 0;
						prevVectNum < 
						numVectsNeg;
						prevVectNum++		
					   ){

						for(
							uintmax_t skipVects = 0;
							skipVects < 
							numClasses -
							2 -
							prevVectNum;
							skipVects++
						   ){
							fseek(
								output,
								vectorSize,
								SEEK_CUR
							     );
						}
					}
					//Seek to current positive vector
					for(
						uintmax_t prevPosVectNum = 0;
						prevPosVectNum < 
						vectNum - numVectsNeg;
						prevPosVectNum++
					   ){
						fseek(
							output,
							vectorSize,
							SEEK_CUR
						     );
					}
				}
				// Find the dot product between the 
				// normalized bitmap and current support vector
				double dotProduct = 0.0d;
				if(height < 0){
					fseek(
						sample,
						sizeof(uint8_t) * 
						offsetToBitmap, 
						SEEK_SET
						);
				}else{
					fseek(
						sample,
						-1 *
						(intmax_t)
						(
						sizeof(uint8_t) * 
						bmpRowSizeBytes
						),
						SEEK_END
					     );
				}
				// Avoid divide by zero by 
				// keeping dot produce zero
				if(sumColorValues > 0){
					for(
						uint64_t rowNum = 0; 
						rowNum < imaxabs(height); 
						rowNum++
					){
						for(
							uint64_t pixNum = 0;
							pixNum < 
							width * bytesPerPixel;
							pixNum++
						){
							double curVectDim;
							fread(
								&curVectDim, 
								sizeof(double), 
								1, 
								output
							);
							uint8_t bmpDim;
							fread(
								&bmpDim, 
								sizeof(uint8_t), 
								1, 
								sample
							);
							dotProduct += 
								(double)bmpDim /
								sumColorValues *
								curVectDim;
						}
						if(height < 0){
							fseek(
								sample,
								-1 *
								(intmax_t)
								(
								sizeof(uint8_t) 
								*
								(
								bmpRowSizeBytes
								+ 
								(
								width 
								*
								bytesPerPixel
								)
								)
								),
								SEEK_CUR
							);
						}else{
							fseek(
								sample,
								sizeof(uint8_t)
								*
								(
								bmpRowSizeBytes
								-
								(
								width 
								* 
								bytesPerPixel	
								)
								),
								SEEK_CUR
							     );
						}
					}
				}
				// If class is a negative sample, 
				// negate dot product
				if(vectNum < numVectsNeg){
					dotProduct = -(dotProduct);
				}
				// Go back to start of support vector and bmp
				if(height < 0){
					fseek(
						sample,
						sizeof(uint8_t) * 
						offsetToBitmap, 
						SEEK_SET
						);
				}else{
					fseek(
						sample,
						-1 *
						(intmax_t)
						(
						sizeof(uint8_t) 
						* 
						bmpRowSizeBytes
						),
						SEEK_END
					     );
				}
				fseek(
					output, 
					-((intmax_t)vectorSize), 
					SEEK_CUR
				);
				if(dotProduct < 1.0d){
					for(
						uint64_t rowNum = 0;
						rowNum < imaxabs(height);
						rowNum++
					   ){
						for(
							uint64_t pixNum = 0;
							pixNum < 
							width * bytesPerPixel;
							pixNum++
						   ){
							double vectorDim;
							fread(
								&vectorDim,
								sizeof(double),
								1,
								output
							     );
							fseek(
								output,
								-1 
								*
								sizeof(double),
								SEEK_CUR
							     );
							uint8_t bmpDim;
							fread(
								&bmpDim,
								sizeof(uint8_t),
								1,
								sample	
							     );
							vectorDim -= 
								learnRate 
								*
								(
								(
								lambda 
								*
								vectorDim
								)
								-
								(double)
								bmpDim 
								/
								bmpRowSizeBytes
								);

							fwrite(
								&vectorDim,
								sizeof(double),
								1,
								output
							      );
						}
						if(width < 0){
							fseek(
								sample,
								-1 
								*
								sizeof(char) 
								*
								(
								bmpRowSizeBytes
								+
								(
								width
								*
								bytesPerPixel
								)
								),
								SEEK_CUR
							     );
						}else{
							fseek(
								sample,
								bmpRowSizeBytes
								-
								(
								width
								*
								bytesPerPixel
								),
								SEEK_CUR
							     );
						}
					}
				}else{
					for(
						uint64_t rowNum = 0;
						rowNum < imaxabs(height);
						rowNum++
					   ){
						for(
							uint64_t pixNum = 0;
							pixNum < 
							width * bytesPerPixel;
							pixNum++
						   ){
							double vectorDim;
							fread(
								&vectorDim,
								sizeof(double),
								1,
								output
							     );
							fseek(
								output,
								-1 
								*
								sizeof(double),
								SEEK_CUR
							     );
							uint8_t bmpDim;
							fread(
								&bmpDim,
								sizeof(uint8_t),
								1,
								sample	
							     );
							vectorDim -= 
								learnRate 
								*
								lambda 
								*
								vectorDim;

							fwrite(
								&vectorDim,
								sizeof(double),
								1,
								output
							      );
						}
						if(width < 0){
							fseek(
								sample,
								-1 
								*
								sizeof(char) 
								*
								(
								bmpRowSizeBytes
								+
								(
								width
								*
								bytesPerPixel
								)
								),
								SEEK_CUR
							     );
						}else{
							fseek(
								sample,
								bmpRowSizeBytes
								-
								(
								width
								*
								bytesPerPixel
								),
								SEEK_CUR
							     );
						}
					}
				}
			fclose(output);
			}
			fclose(sample);
			fprintf(stderr, "\r");
		}
	}
	return true;
}

// Classify a file using a premade SVM file
bool classifyFileFromSvm(
		char *pathToInputFile,
		char *pathToSvmFile
		){
	uint32_t width;
	int32_t height;
	uint16_t bitsPerPixel;
	if(access(pathToInputFile, R_OK) != 0){
		fprintf(
			stderr,
			"Insufficient permission to read %s\n",
			pathToInputFile
		       );
		return false;
	}
	if(!isBmpFile(
		pathToInputFile,
		&width,
		&height,
		&bitsPerPixel
		)){
		fprintf(
			stderr,
			"Could not identify %s as a suitable BMP file\n",
			pathToInputFile
		       );
		return false;
	}
	return true;
}

int main(int argc, char **argv){
	bool *firstArgIsDir = (bool *)(malloc(sizeof(bool)));
	if(!validArgs(argc, argv, firstArgIsDir)){
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if(*firstArgIsDir){
		if(!createSvmFromDir(argv[1], argv[2])){
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}else{
		if(!classifyFileFromSvm(argv[1], argv[2])){
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	
	fprintf(
		stderr,
		"Task completed successfully\n"
	       );
	exit(EXIT_SUCCESS);
}
