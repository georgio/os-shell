dependencies:
	@/bin/echo -n "Compiling dependency: libargon2"
	@cd etc/phc-winner-argon2; make
	@/bin/echo "                   OK"

build:
	@/bin/echo -n "Compiling mysh"
	@gcc -O3 -w -c main.c -o main.o
	@gcc -O3 -w -o mysh main.o -largon2;
	@/bin/echo "                   OK"

init:
	@/bin/echo -n "Initializing"
	@cd etc; /bin/echo -n "0" > util; /bin/echo -n "" > passwd
	@/bin/echo "                   OK"
	
clean:
	@/bin/echo -n "Cleaning up..."
	@cd etc/phc-winner-argon2; make clean
	@$(RM) mysh
	@$(RM) main.o
	@/bin/echo "                   OK"

.PHONY: dependencies build clean init
