include ../common.mk

TARGET = npshell
OBJ = main.o command.o exec.o env.o

all: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)
	cp $(TARGET) ../

clean:
	$(RM) $(OBJ) $(TARGET)
	$(RM) ../$(TARGET)

%.o: %.c %.h command.h debug.h
	$(CC) $(CFLAGS) -c $< -o $@
	
