all: serverA.cpp serverB.cpp servermain.cpp client.cpp
	g++ -std=c++11 serverA.cpp -o serverA

	g++ -std=c++11 serverB.cpp -o serverB

	g++ -std=c++11 servermain.cpp -o mainserver
	
	g++ -std=c++11 client.cpp -o client

.PHONY: serverA
serverA: 
	./serverA

.PHONY: serverB
serverB:
	./serverB

.PHONY: mainserver
mainserver:
	./mainserver

.PHONY: client
client:
	./client

