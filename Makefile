.PHONY: clean

stnc: stnc.c
	gcc -Wall -o stnc stnc.c

clean:
	rm -f stnc