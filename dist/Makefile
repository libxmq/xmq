example: example.c xmq.c xmq.h
	gcc -g -O0 -o example example.c xmq.c -lm

examplecc: example.c xmq.c xmq.h
	g++ -o examplecc example.c xmq.c -lm

example_libso: example.c
	gcc -g -O0 -Wl,-rpath=/usr/local/lib -o example_libso example.c -lxmq -lm

example_liba: example.c
	gcc -g -O0 -o example_liba example.c /usr/local/lib/libxmq.a -lm
