make: main.c
	gcc -s -o main main.c

debug:
	gcc -g -o main main.c
	gdb ./main

clean:
	rm -f main

run:
	make clean
	make
	clear
	./main
