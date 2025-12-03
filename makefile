GCC = gcc
LFLAGS = 
CFLAGS = -std=c99

EXEC = a.out
SOURCE = main.c

vfs: $(SOURCE)
	$(GCC) $(SOURCE) $(CFLAGS) $(LFAGS)

run:
	./$(EXEC)
clean:
	rm $(EXEC)

