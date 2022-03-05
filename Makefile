build:
	clang++ main.cpp helloworld.pb.cc -L/usr/local/lib -I/usr/local/include/grpc -lgrpc++_reflection -lgrpc++ -lgrpc -lprotobuf -lpthread -o grpc_core_server
build_sani:
	clang++ -fsanitize=undefined main.cpp helloworld.pb.cc -L/usr/local/lib -I/usr/local/include/grpc -lgrpc++_reflection -lgrpc++ -lgrpc -lprotobuf -lpthread -o grpc_core_server
clean:
	rm grpc_core_server
exec:
	./grpc_core_server
