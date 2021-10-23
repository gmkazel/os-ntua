fconc: main.o functions.o
	gcc -Wall -Werror main.o functions.o -o fconc

main.o: main.c
	gcc -Wall -Werror -c main.c

functions.o: functions.c
	gcc -Wall -Werror -c functions.c

clean:
	rm -f *.o fconc *.out
