## This is a simple Makefile

# Define what compiler to use and the flags.
CC=cc
CXX=CC
CCFLAGS= -g -std=c99 -Wall -Werror


all: hw3_solution

# Compile all .c files into .o files
# % matches all (like * in a command)
# $< is the source file (.c file)
%.o : %.c
	$(CC) -c $(CCFLAGS) $<


hw3_solution: hw3_solution.o
	$(CC) -o hw3_solution hw3_solution.o -lm


clean:
	rm -f *.o hw3_solution
