CHDS: CHDSmain.o cfg.o log.o
	gcc -o CHDS CHDSmain.o cfg.o log.o

CHDSmain.o: CHDSmain.c cfg.c log.c
	gcc -c CHDSmain.c cfg.c log.c
cfg.o: cfg.c
	gcc -c cfg.c
log.o: log.c
	gcc -c log.c

clean:
	rm CHDS CHDSmain.o cfg.o log.o
