#This is a Makefile for the Date example 
CC = gcc 
CFLAGS = -Wall -g 
CLINK = $(CC)
OBJS =  main.o    
RM = rm -f 
#Default target (usually "all") 
all: ttftps 
#Creating the executables
ttftps: $(OBJS) 
	$(CLINK) -o ttftps $(OBJS) 
#Creating object files using default rules 
main.o: main.c 
#Cleaning old files before new make 
clean:
	$(RM) ttftps *.o *.bak *~ "#"* core
