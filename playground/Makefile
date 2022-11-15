all: build run clean

build:
	gcc ../template/tm.c playground.c -I ../include -o playground -g

clean: 
	rm playground
run:
	./playground
debug:
	lldb -S ./lldb_start.txt