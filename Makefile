build:
	clang++ main.cpp -L/usr/local/lib -I/usr/local/include/grpc -lgrpc -o grpc_core_server
clean:
	rm grpc_core_server
exec:
	./grpc_core_server
