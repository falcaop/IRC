server:
	gcc server.c -o server.out
	./server.out
	
client:
	gcc client.c -o client.out
	./client.out

clear:
	rm -f *.out