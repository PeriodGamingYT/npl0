make: main.c
	gcc -s -o main main.c

clean:
	rm -f main

run:
	make clean
	make
	clear
	./main

lint:
	clear
	cppcheck --enable=all main.c
gdb:
	gcc -g -O0 -o main main.c
	cat test.npl | xclip -sel clip
	clear
	gdb ./main
