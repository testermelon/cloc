all: main.c
	gcc -Wall main.c -o cloc `pkg-config --cflags --libs xcb cairo` -pthread

test: cloc
	cp cloc ~/housekeeping/cloc_c
	~/housekeeping/cloc.sh
