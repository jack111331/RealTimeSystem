all: simple_processes simple_threads thread_task priority_inversion priority_ceiling priority_inheritance

simple_processes: simple_processes.cpp
	g++ -o simple_processes simple_processes.cpp
	
simple_threads: simple_threads.cpp
	g++ -lpthread -o simple_threads simple_threads.cpp
	
thread_task: thread_task.cpp
	g++ -lpthread -o thread_task thread_task.cpp

priority_inversion: ./Priority_Inversion/priority_inversion.cpp
	g++ -lpthread -o ./Priority_Inversion/priority_inversion ./Priority_Inversion/priority_inversion.cpp

priority_ceiling: ./Priority_Inversion/priority_ceiling.cpp
	g++ -lpthread -o ./Priority_Inversion/priority_ceiling ./Priority_Inversion/priority_ceiling.cpp

priority_inheritance: ./Priority_Inversion/priority_inheritance.cpp
	g++ -lpthread -o ./Priority_Inversion/priority_inheritance ./Priority_Inversion/priority_inheritance.cpp
