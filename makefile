all: ServiForo CliForo

ServiForo: ServiForo.o
	gcc -o ServiForo ServiForo.o -lpthread -lrt

CliForo: CliForo.o
	gcc -o CliForo CliForo.o -lpthread -lrt

ServiForo.o: ServiForo.c 
	gcc -c ServiForo.c -Wall

CliForo.o: CliForo.c 
	gcc -c CliForo.c -Wall

clean: 
	rm ServiForo CliForo ServiForo.o CliForo.o