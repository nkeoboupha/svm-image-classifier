# Nate's SVM

This C program uses a directory with subdirectories containing BMP files to train an output file which defines a multiclass support vector machine. Once a file is trained, you may run the program again to classify images into one of the classes 
on which the file was trained.

## Usage

### Defining Macros

Before compiling, it is recommended to tailor the four macros towards the top of the program to the parameters 
that you wish to train the file with and the level of verbosity that you wish. They should be just past the `#include` 
directives and look similar to the following:

```
#define NUM_STEPS 792000
#define STEP_REPORT_INTERVAL 100
#define LAMBDA 0.0001
// Debug = 0, Info = 1, Off >= 2
#define DEBUG_LEVEL 1
```
#### `NUM_STEPS`

Increasing `NUM_STEPS` should increase the quality of support vectors, albeit with diminishing returns. Keep in mind 
that the time required to train a file increases linearly with `NUM_STEPS` and the total size of the BMP data, but 
quadratically with the number of classes.

#### `LAMBDA`

With each sample that a relevant vector is trained on, the vector is reduced in magnitude by a proportion determined by the product of `LAMBDA` and the current training rate. Smaller values encourage more accurate classification and larger values emphasise greater margins between the classes.

#### `STEP_REPORT_INTERVAL`

When `DEBUG_LEVEL` is `1`, the program reports the number of completed steps before every `STEP_REPORT_INTERVAL` steps.

#### `DEBUG_LEVEL`

When `DEBUG_LEVEL` is defined as `0`, all diagnostic data is output. At `1`, only the most notable updates are 
output. At `2` or above, only notice of the completion of training or the results of classification are output. 

Note that lower debug levels may incur performance losses.

### Compiling

The C file can be complied with no additional dependencies beyond the C standard library and the C POSIX library, 
although `libm` must be linked by passing the `-lm` argument.
Assuming that `nsvm.c` is in your current working directory, you can compile it into an executable with the 
following command:

`gcc -o nsvm nsvm.c -lm`

The executable can now be run in one of two ways:

### Making a file containing the support vectors

`./nsvm <Path to directory> <Path to output vector file>`

The above takes a directory containing class subdirectories and either creates or overwrites a binary 
file containing the support vectors and metadata required to classify BMP files using the command in the 
following section.

A class subdirectory should contain files adhering to the BMP file format in its top level, as the program does not 
currently search for such files recursively.

All such BMP files across all subdirectories must have the same width, height, and number of bytes per pixel. 
Files that use compression or that don't have a whole number of bytes per pixel are not currently supported; 
however, files with positive and negative heights can be used interchangeably as long as they both have the same 
absolute value.

Subdirectories should ideally have a name that appropriately represents the contained BMP files 
(e.g. animal, building, car, etc.)

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

Note that it is not necessary for the files to have the `.bmp` extension to be considered for training.

### Using the file containing the support vectors

`./nsvm <Path to BMP file> <Path to input vector file>`

The above classifies a file adhering to the BMP format using a binary file produced using the command in the 
previous section.

This BMP file must have the same width, height, and number of bytes per pixel as those used in training. 
A file with the same height value, except negative, is acceptable by the program.

The program will use the support vectors and metadata contained in the binary file to vote for which class best
categorizes the content of the input BMP file.

A class (or classes in the result of a tie) will be output, along with the percentage confidence.

## Limitations

* Development was conducted with the reasonable assumption that both training and classification would occur 
on little-endian systems that use 8-bit bytes and define `double` similarly
* Degraded accuracy may occur when using images that do not use one byte per color channel
