make: main.c
	gcc -s -o main main.c

clean:
	rm -f main

run:
	make clean
	make
	clear
	./main
