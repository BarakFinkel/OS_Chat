.PHONY: clean

stnc: stnc1.c
	gcc -Wall -o stnc stnc1.c -pthread

clean:
	rm -f stnc