CC=gcc
DEPS = constants.h sdd.h

sim: sim_methods.c simulator.c sdd.c $(DEPS)
	$(CC) -o sim sim_methods.c simulator.c sdd.c

generator: task_input_generator.c
	$(CC) -o generator task_input_generator.c
