make: main.c
	gcc -s -o main main.c

gdb:
	gcc -g -O0 -o main main.c
	clear
	gdb ./main

val:
	gcc -g -o main main.c
	clear
	valgrind  --trace-children=yes --track-fds=yes --track-origins=yes --leak-check=full --show-leak-kinds=all ./main

clean:
	rm -f main

run:
	make clean
	make
	clear
	cat test.npl | ./main

repl:
	make clean
	make
	clear
	./main
