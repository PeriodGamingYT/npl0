make: main.c
	gcc -s -o main main.c

gdb:
	gcc -g -o main main.c
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
	echo 'b32- x = -1 bptr+ y = :x #b32- y' | ./main
