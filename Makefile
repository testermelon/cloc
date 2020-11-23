all: main.c
	gcc -Wall main.c -o cloc `pkg-config --cflags --libs xcb cairo`

