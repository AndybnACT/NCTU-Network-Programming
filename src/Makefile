include ../common.mk

TARGET = npshell
OBJ = main.o command.o exec.o env.o

all:
	make server1
	mv server1 ../

server1: $(OBJ) np_ccos.o
	$(CC) $(CFLAGS) $(OBJ) np_ccos.o -o server1

clean:
	$(RM) $(OBJ) $(TARGET)
	$(RM) ../$(TARGET)

%.o: %.c %.h command.h debug.h
	$(CC) $(CFLAGS) -c $< -o $@
	
