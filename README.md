# Nate's SVM

This project is a work-in-progress that will result in a C implementation of a multiclass support vector machine, 
with the goal of making image-classification machine learning tasks somewhat more accessible. Currently, this 
README serves the purpose of something like a requirements document.

## What to expect upon public release

The C file can be complied with no additional dependencies using a compiler such as `gcc`.
Assuming that `nsvm.c` is in your current working directory, you can compile it into an executable with the 
following command:

`gcc -o nsvm nsvm.c`

The executable can now be run in one of two ways:

### Making a file containing the support vectors

`./nsvm <Path to directory> <Path to output vector file>`

The above takes a directory containing appropriate subdirectories and either creates or overwrites a binary 
file containing the support vectors and metadata required to classify BMP files using the command in the 
following section.

An appropriate subdirectory contains files adhering to the BMP file format.

All such BMP files accross all subdirectories must have the same height, width, and number of bytes per pixel (i.e.
they either all support transparency or none do).

Subdirectories should ideally have a name that appropriately represents the contained BMP files (e.g. animal, building, car, etc.)

The following is an example of a directory called `Sample Classes` that would be appropriate to pass the path of as an argument to the program:

```bash
Sample Classes
├── animal
│   ├── bonobo.bmp
│   ├── koala.bmp
│   └── platypus.bmp
├── building
│   ├── hotel.bmp
│   ├── office.bmp
│   └── warehouse.bmp
└── car
    ├── 126.bmp
    ├── Focus.bmp
    └── Isetta.bmp
```

### Using the file containing the support vectors

`./nsvm <Path to BMP file> <Path to input vector file>`

The above classifies a file adhering to the BMP format using a binary file produced using the command in the 
previous section.

This BMP file must have the same height, width, and number of bytes per pixel as those used in training.

Furthermore, the machine on which classification is performed must define `long double` to have the same size 
as one the machine which performed training. This should not present a problem if both tasks are performed on 
the same machine or if both machines use the same instruction set architecture.

The program will use the support vectors and metadata contained in the binary file to vote for which class best
categorizes the content of the input BMP file.

A class (or classes in the result of a tie) will be output, along with the percentage confidence.

## Current Functionality

The program will take and validate that the two input arguments are of the correct type and quantity. 
In the event that invalid paths are submitted, the program will terminate after displaying an error message.

