# Makefile, versao 1
# Sistemas Operativos, DEI/IST/ULisboa 2020-21

CC   = gcc
LD   = gcc
CFLAGS = -Wall -std=gnu99 -I../
LDFLAGS=-lm

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean run

all: tecnicofs

tecnicofs: stack.o fs/state.o fs/operations.o main.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs fs/state.o fs/operations.o main.o stack.o -lpthread

stack.o: stack.c stack.h
	$(CC) $(CFLAGS) -o stack.o -c stack.c

fs/state.o: fs/state.c fs/state.h tecnicofs-api-constants.h stack.h
	$(CC) $(CFLAGS) -o fs/state.o -c fs/state.c -lpthread

fs/operations.o: fs/operations.c fs/operations.h fs/state.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -o fs/operations.o -c fs/operations.c -lpthread

main.o: main.c fs/operations.h fs/state.h tecnicofs-api-constants.h stack.h
	$(CC) $(CFLAGS) -o main.o -c main.c -lpthread

clean:
	@echo Cleaning...
	rm -f fs/*.o *.o tecnicofs

run: tecnicofs
	./tecnicofs
