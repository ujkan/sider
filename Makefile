main:
	gcc -Wall -pedantic -fsanitize=address -O0 -g data-structs/hmap.c utils/bytering.c client-server/server_v2.c -I./utils/ -I./data-structs/ -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -lglib-2.0 -o server_v2_debug
	g++ -O0 -g client-server/client-latest.cpp -o client

v2:
	gcc -O0 -g server_v2.c -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -lglib-2.0 -o server_v2_debug
	g++ -O0 -g client.cpp -o client
