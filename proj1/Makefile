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

tecnicofs: lock.o fs/state.o fs/operations.o main.o
	$(LD) $(CFLAGS) $(LDFLAGS) -o tecnicofs fs/state.o fs/operations.o main.o lock.o -lpthread

lock.o: lock.c lock.h
	$(CC) $(CFLAGS) -o lock.o -c lock.c -lpthread

fs/state.o: fs/state.c fs/state.h tecnicofs-api-constants.h lock.h
	$(CC) $(CFLAGS) -o fs/state.o -c fs/state.c -lpthread

fs/operations.o: fs/operations.c fs/operations.h fs/state.h tecnicofs-api-constants.h
	$(CC) $(CFLAGS) -o fs/operations.o -c fs/operations.c -lpthread

main.o: main.c fs/operations.h fs/state.h tecnicofs-api-constants.h lock.h
	$(CC) $(CFLAGS) -o main.o -c main.c -lpthread

clean:
	@echo Cleaning...
	rm -f fs/*.o *.o tecnicofs output*

run: tecnicofs
	./tecnicofs
