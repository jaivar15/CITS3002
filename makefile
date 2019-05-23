PROJECT =  server
HEADERS =  $(PROJECT).h
OBJ 	=  main.o server.o game.o

CC = gcc 
CFLAGS	=  -w

$(PROJECT) : $(OBJ)
	$(CC) $(CFLAGS) -o  $(PROJECT) $(OBJ) -lm

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(PROJECT) $(OBJ) 
