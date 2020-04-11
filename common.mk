RM = rm
CXX = g++
CC  = gcc
CFLAGS = -Wall -g

WORKDIR = working_dir

%: %.cpp
	$(CXX) $(CFLAGS) $< -o $@

%:%.c
	$(CC) $(CFLAGS) $< -o $@