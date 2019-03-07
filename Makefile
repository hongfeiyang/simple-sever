CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -lpthread
OBJ = server.o 
EXE = server

##Create .o files from .c files. Searches for .c files with same .o names given in OBJ
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

##Create executable linked file from object files. 
$(EXE): $(OBJ)
	gcc -o $@ $^ $(CFLAGS)


##Performs clean (i.e. delete object files) and deletes executable
clean:
	/bin/rm $(OBJ)
	/bin/rm $(EXE) 
