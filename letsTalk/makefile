all: list.c lets-talk.c
	gcc lets-talk.c list.c -pthread -Wall -o lets-talk
	
Valgrind:
	valgrind ./lets-talk 3000 127.0.0.1 3001
	
clean:
	rm *.o lets-talk
