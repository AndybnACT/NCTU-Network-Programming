include ../common.mk

OBJ = main.o command.o exec.o env.o net.o np_ccos.o np_single_proc.o msg.o upipe.o

all:
	echo "#define CONFIG_SERVER1" > _config.h
	make server
	cp server ../server1
	make clean
	
	echo "#define CONFIG_SERVER3" > _config.h
	make server3
	cp server ../server3
	make clean
	
	echo "#define CONFIG_SERVER2" > _config.h
	make server
	cp server ../server2
	make clean

server: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o server
	
server3: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -lrt -o server

clean:
	$(RM) $(OBJ)
	$(RM) server

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	
