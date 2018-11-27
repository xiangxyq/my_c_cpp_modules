all:
	gcc -c playback.c -I. -lasound -o playback.o
	ar crv libplayback.a playback.o
	gcc playback.c -I. -fPIC -shared -o libplayback.so
	gcc test_main.cc playback.c -I. -lasound -o test_main

clean:
	rm -rf *.o test_main *.a *.so
