zing: main.o zing.o zing2.o
	gcc -Wall main.o zing.o -o zing
	gcc -Wall main.o zing2.o -o zing2

main.o: main.c
	gcc -Wall -c main.c

zing2.o: zing2.c
	gcc -Wall -c zing2.c

clean:
	rm -f main.o zing2.o zing zing2
