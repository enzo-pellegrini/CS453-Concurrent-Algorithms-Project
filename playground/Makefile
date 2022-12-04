all: build run clean

build:
	gcc ../353607/tm.c ../353607/versioned_lock.c ../353607/bloom.c playground.c -I ../include -o playground -g

clean: 
	rm playground
run:
	./playground
debug:
	lldb playground
