CC=gcc
CFLAGS= -g -Wall -MMD -MP

CXX=g++
CXXFLAGS= -g -Wall


TARGET=socks_server
OBJ=main.o socks.o socket.o


DEP=$(OBJ:%.o=%.d)

.PHONY: all clean

all: $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

-include $(DEP)

clean:
	rm $(OBJ)
	rm $(TARGET)
	rm $(DEP)
	
%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@
