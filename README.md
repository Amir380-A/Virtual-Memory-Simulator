# Virtual-Memory-Simulator

# Discription
This is a Virtual Memory Simulator that Simulate the relationship between the storage devises with the CPU based on Von Neuman architectual Model and how do they translate between the logical and Physical addresses. 

This Project uses C and simulates Virtual Memory Management. 
it simulates things like, Backing store, RAM, Becuse we can't simulate the Actual CPU, we provided the Logical Addresses based on a text file input.

The Project uses 4 diffrent page Replacement algorithms like FIFO, LRU, MFU and LFU.


## How to Compile and run

To Compile this project, you first need to install gcc compiler, you can use any linux machine or a VM.

you can use two ways to compile it, first 

```bash
   gcc -o vm_sim vm_sim.c vmtypes.c
```
Or use can compile it using the makefile
```bash
   make
```

 To execute the project, from the command line type `$ ./vm_sim InputFile.txt` or use an appropriate `[input].txt` file with your Logical addresses.
