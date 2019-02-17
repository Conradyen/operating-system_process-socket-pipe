all:Admin Client

Admin: admin.cpp
	g++  -lpthread -o Admin admin.cpp -std=c++11
Client: client.cpp
	g++ -o Client client.cpp -std=c++11
clean:
	rm Admin Client
