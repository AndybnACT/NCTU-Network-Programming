include ../common.mk
TARGETDIR = ../$(WORKDIR)/bin/

OBJ = noop number removetag removetag0 print_fds

all: $(OBJ)
	cp $(OBJ) $(TARGETDIR)

clean:
	$(RM) $(OBJ)
	$(RM) $(addprefix $(TARGETDIR), $(OBJ))