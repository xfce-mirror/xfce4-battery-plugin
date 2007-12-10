# $Id: $

CC = gcc
CFLAGS = -pipe -O -Wall -Werror

simulator: simulator.c
	$(CC) $(CFLAGS) simulator.c -o simulator \
 	`pkg-config gtk+-2.0 --cflags --libs`\
	`pkg-config hal --cflags --libs` \
	`pkg-config dbus-glib-1 --cflags --libs` \
	`pkg-config libxfcegui4-1.0 --cflags --libs`

clean:
	rm -f *.o simulator
