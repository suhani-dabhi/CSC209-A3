#ifndef PORT
    #define PORT 59804
#endif

all: battle

battle: battle.c
        gcc -o battle battle.c

clean: rm -f battle
