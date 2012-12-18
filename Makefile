
CFLAGS = -O2 -Wall -g
LFLAGS = -g

.SUFFIXES: .o .c

all: asmq1 q1sim

asmq1: src/asmq1.o
	$(CC) $(LFLAGS) -o asmq1 $<

q1sim: src/q1sim.o
	$(CC) $(LFLAGS) -o q1sim $<

.c.o: $*.o
	$(CC) $(CFLAGS) -c -o $*.o $*.c

clean:
	rm -f asmq1 q1sim src/*.o

