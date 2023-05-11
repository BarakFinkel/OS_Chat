.PHONY: clean

stnc: stnc.c
	gcc -Wall -o stnc stnc.c -pthread

clean:
	rm -f stnc