
example: example.c xmq.c xmq.h
	gcc -g -O0 -I/usr/include/libxml2 -o example example.c xmq.c -lxml2

examplecc: example.c xmq.c xmq.h
	g++ -I/usr/include/libxml2 -o examplecc example.c xmq.c -lxml2

example_libso: example.c
	gcc -g -O0 -I/usr/include/libxml2 -Wl,-rpath=/usr/local/lib -o example_libso example.c -lxmq -lxml2

example_liba: example.c
	gcc -g -O0 -I/usr/include/libxml2 -o example_liba example.c /usr/local/lib/libxmq.a -lxml2
