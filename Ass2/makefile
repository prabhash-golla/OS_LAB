run: parent child dummy
	./parent 11

parent: parent.c
	gcc -Wall -o parent parent.c

child: child.c
	gcc -Wall -o child child.c

dummy: dummy.c
	gcc -Wall -o dummy dummy.c

clean:
	-rm -f parent child dummy childpid.txt dummycpid.txt

