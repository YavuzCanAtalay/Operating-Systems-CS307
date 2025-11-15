CC = gcc
CFLAGS = -Wall -Wextra -O2

all: shell 

shell: shell.c parser.c mpc.c parser.h mpc.h
	$(CC) $(CFLAGS) shell.c parser.c mpc.c -o shell $(LDLIBS)

clean:
	rm -f shell 