all: client.c server.c
	gcc client.c -o WTF -lssl -lcrypto -g
	gcc server.c -o WTFserver -pthread
clean:
	rm WTF 
	rm WTFserver
	rm WTFtest
	rm -rf user1
	rm -rf user2
	rm -rf server
test: WTFtest.c
	gcc WTFtest.c -o WTFtest
