
CFLAGS = -O2 -Wall -g
LFLAGS = -g

OBJECTS = src/main.o

all: aq1

aq1: $(OBJECTS)
	$(CC) $(LFLAGS) -o aq1 $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $*.c

