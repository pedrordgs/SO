CC = gcc
CFLAGS = -Wall -g -ggdb

program: ma sv cv ag

ma: lib
	$(CC) $(CFLAGS) ma.c -o ma lib.o

sv: lib
	$(CC) $(CFLAGS) sv.c -o sv lib.o

cv: lib
	$(CC) $(CFLAGS) cv.c -o cv lib.o

ag: lib
	$(CC) $(CFLAGS) ag.c -o ag lib.o

lib:
	$(CC) $(CFLAGS) -c lib.c

clean:
	rm ma sv cv ag lib.o

cleanfich:
	rm stock vendas artigos strings ./help/maisvendidos
