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
	mpirun -oversubscribe -np 4 ./main -g 2 -c 2 | tee out.log
