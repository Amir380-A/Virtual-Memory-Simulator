CC=gcc
CFLAGS=-I.

vmtypes.o: vmtypes.c vmtypes.h
$(CC) -c -o vmtypes.o vmtypes.c $(CFLAGS)

vm_sim.o: vm_sim.c vmtypes.h
$(CC) -c -o vm_sim.o vm_sim.c $(CFLAGS)

vm_sim: vmtypes.o vm_sim.o
$(CC) -o vm_sim vm_sim.o vmtypes.o $(CFLAGS)