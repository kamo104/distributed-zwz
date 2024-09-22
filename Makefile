SOURCES := src/*
HEADERS := include
CC := mpicxx
FLAGS=-DDEBUG -g
# FLAGS=-g

all: main

main: $(SOURCES) $(HEADERS)
	mpicxx -std=c++20 -I$(HEADERS) $(SOURCES) $(FLAGS) -o main

clear: clean

clean:
	rm main a.out


# mpiexec ??
run: main
	mpirun -oversubscribe -np 2 -oversubscribe ./main -g 1 -c 2
