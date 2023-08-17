make: main.c
	gcc -s -o main main.c

gdb:
	gcc -g -o main main.c
	clear
	gdb ./main

val:
	gcc -g -o main main.c
	clear
	valgrind ./main

clean:
	rm -f main

run:
	make clean
	make
	clear
	./main
