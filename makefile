GCC = gcc
LFLAGS = 
CFLAGS = 

EXEC = a.out
SOURCE = main.c

vfs: 
	$(GCC) $(SOURCE) $(CFLAGS) $(LFAGS)

run:
	./$(EXEC)
clean:
	rm $(EXEC)

