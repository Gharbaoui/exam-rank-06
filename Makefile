practice:  practice.c
	gcc  practice.c -g  -o  practice

clean:
	rm -f practice

re: clean practice
