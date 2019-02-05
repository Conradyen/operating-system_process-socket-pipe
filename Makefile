all:admin client

admin: admin.cpp
	g++ -o admin admin.cpp
client: client.cpp
	g++ -o client client.cpp 
