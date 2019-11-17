all:	np_simple.cpp
	g++ -o np_simple np_simple.cpp
serv2:
	g++ -o np_single_proc np_single_proc.cpp
serv3:
	g++ -o np_multi_proc np_multi_proc.cpp
