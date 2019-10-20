all:
	make -C ./commands
	make -C ./src
clean:
	make -C ./commands clean 
	make -C ./src clean
	