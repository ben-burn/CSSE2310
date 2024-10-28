# Define the compiler.
CC = gcc
# Define the flags for compilation. // all under here is for linking library
CFLAGS = -Wall -pedantic -std=gnu99 -g
# Define additional flags for debugging purposes.
DEBUG = -g

# Define the location and name of the dynamic library .so file and LDFLAGS here
# The gcc -L flag looks in the specified directory for library (.so) files
# The gcc -l flag links with the specified library file
# We assume the following file exists:
# dynamic-library-so/libdynamic-library.so
LIBDIR = /local/courses/csse2310/lib 
LDFLAGS = -L$(LIBDIR) -lcsse2310a1

# Define the location of the dynamic library header (.h) file here and add it to CFLAGS
# The gcc -I flag looks in the specified directory for header (.h) files
# We assume the following file exists:
# dynamic-library-inc/dynamic-library.h
INCLUDEDIR = /local/courses/csse2310/include
CFLAGS += -I$(INCLUDEDIR)

# Specify the default target to run.
.DEFAULT_GOAL := uqwordladder

# Specify which targets do not generate output files.
.PHONY: debug clean

# The debug target will update compile flags then 
# clean and compile program.
debug: CFLAGS += $(DEBUG)
debug: clean uqwordladder

# program.o is the target and program.c is the dependency.
program.o: uqwordladder.c
	# This is the recipe for compiling program.o which is an object file.
	# Use the -c flag to compile but not link.
	# $^ is the set of dependent files.
	# $@ is the name of the target to be built.
	# The line below is equivalent to gcc -Wall -pedantic -std=gnu99 -c program.c -o program.o
	$(CC) $(CFLAGS) -c $^ -o $@

# program is the target and program.o is the dependency.
program: uqwordladder.o
	# This is the recipe for linking program which is an executable file.
	# The line below is equivalent to gcc -Wall -pedantic -std=gnu99 program.o -o program
	$(CC) $(CFLAGS) $^ -o $@

# Remove object and binary files.
clean:
	rm -f uqwordladder *.o
