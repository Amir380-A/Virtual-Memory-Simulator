#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>
#include <time.h>
#include "vmtypes.h"

#define FRAME_SIZE        256       // Size of each frame
#define TOTAL_FRAME_COUNT 256       // Total number of frames in physical memory
#define PAGE_MASK         0xFF00    // Masks everything but the page number
#define OFFSET_MASK       0xFF      // Masks everything but the offset
#define SHIFT             8         // Amount to shift when bitmasking
#define TLB_SIZE          30        // size of the TLB
#define PAGE_TABLE_SIZE   256       // size of the page table
#define MAX_ADDR_LEN      10        // The number of characters to read for each line from input file.
#define PAGE_READ_SIZE    256       // Number of bytes to read

typedef enum { false = 0, true = !false } bool; 



vmTable_t* tlbTable; // The TLB Structure
vmTable_t* pageTable; // The Page Table
int** dram; // Physical Memory

char algo_choice; // menu stdin
char display_choice; // menu stdin


int nextTLBentry = 0; // Tracks the next available index of entry into the TLB
int nextPage = 0;  // Tracks the next available page in the page table
int nextFrame = 0; // Tracks the next available frame TLB or Page Table

// input file and backing store
FILE* address_file;
FILE* backing_store;

// how we store reads from input file
char addressReadBuffer[MAX_ADDR_LEN];
int virtual_addr;
int page_number;
int offset_number;

// Generating length of time for a function
clock_t start, end;
double cpu_time_used;
int functionCallCount = 0;

// the buffer containing reads from backing store
signed char fileReadBuffer[PAGE_READ_SIZE];

// the translatedValue of the byte (signed char) in memory
signed char translatedValue;


// Function Prototypes
void translateAddress();
void readFromStore(int pageNumber);
void tlbFIFOinsert(int pageNumber, int frameNumber);
void tlbLRUinsert(int pageNumber, int frameNumber);
void tlbLFU_MFUinsert(int pageNumber, int frameNumber);
int getOldestEntry(int tlbSize);
int getMostFrequentEntry(int tlbSize);
int getLessFrequentEntry(int tlbSize);
double getAvgTimeInBackingStore();



int main(int argc, char *argv[])
{
    tlbTable = createVMtable(TLB_SIZE); // The TLB Structure
    pageTable = createVMtable(PAGE_TABLE_SIZE); // The Page Table
    dram = dramAllocate(TOTAL_FRAME_COUNT, FRAME_SIZE); // Physical Memory
    int translationCount = 0;
    char* algoName;




    backing_store = fopen("BACKING_STORE.bin", "rb");

    if (backing_store == NULL) {
        fprintf(stderr, "Error opening BACKING_STORE.bin %s\n","BACKING_STORE.bin");
        return -1;
    }

 
    address_file = fopen(argv[1], "r");

    if (address_file == NULL) {
        fprintf(stderr, "Error opening %s. Expecting [InputFile].txt or equivalent.\n",argv[1]);
        return -1;
    }

    printf("\nNumber of logical pages: %d\nPage size: %d bytes\nPage Table Size: %d\nTLB Size: %d entries\nNumber of Physical Frames: %d\nPhysical Memory Size: %d bytes", PAGE_TABLE_SIZE, PAGE_READ_SIZE, PAGE_TABLE_SIZE, TLB_SIZE, FRAME_SIZE, PAGE_READ_SIZE * FRAME_SIZE);




    do {
        printf("\nChoose TLB Replacement Strategy [1: FIFO, 2: LRU, 3:LFU, 4:MFU]: ");
        scanf("\n%c", &algo_choice);
    } while (algo_choice != '1' && algo_choice != '2' && algo_choice != '3' && algo_choice != '4');




    while (fgets(addressReadBuffer, MAX_ADDR_LEN, address_file) != NULL) {
        virtual_addr = atoi(addressReadBuffer); // converting from ascii to int


        page_number = getPageNumber(PAGE_MASK, virtual_addr, SHIFT);


        offset_number = getOffset(OFFSET_MASK, virtual_addr);


        translateAddress(algo_choice);
        translationCount++;  
    }

   
    if (algo_choice == '1') {
        algoName = "FIFO";
    }
    else if (algo_choice == '2'){
        algoName = "LRU";
    }
    else if (algo_choice == '3'){
        algoName = "LFU";
    }
    else if (algo_choice == '4'){
        algoName = "MFU";
    }


    printf("\n-----------------------------------------------------------------------------------\n");
    // calculate and print out the stats
    printf("\nResults Using %s Algorithm: \n", algoName);
    printf("Number of translated addresses = %d\n", translationCount);
    double pfRate = (double)pageTable->pageFaultCount / (double)translationCount;
    double TLBRate = (double)tlbTable->tlbHitCount / (double)translationCount;

    //printf("Page Faults = %d\n", pageTable->pageFaultCount);
    //printf("Page Fault Rate = %.3f %%\n",pfRate * 100);
    printf("TLB Miss = %d\n", tlbTable->tlbMissCount);
    printf("TLB Hits = %d\n", tlbTable->tlbHitCount);
    printf("TLB Hit Rate = %.3f %%\n", TLBRate * 100);
    printf("Average time spent retrieving data from backing store: %.3f millisec\n", getAvgTimeInBackingStore());
    printf("\n-----------------------------------------------------------------------------------\n");

    // close the input file and backing store
    fclose(address_file);
    fclose(backing_store);

    // NOTE: REMEMBER TO FREE DYNAMICALLY ALLOCATED MEMORY
    freeVMtable(&tlbTable);
    freeVMtable(&pageTable);
    freeDRAM(&dram, TOTAL_FRAME_COUNT);
    return 0;
}


void translateAddress()
{
    // First try to get page from TLB
    int frame_number = -1; // Assigning -1 to frame_number means invalid initially

    /*
        Look through the TLB to see if memory page exists.
        If found, extract frame number and increment hit count
    */
    for (int i = 0; i < tlbTable->length; i++) {
        if (tlbTable->pageNumArr[i] == page_number) {
            frame_number = tlbTable->frameNumArr[i];
            tlbTable->tlbHitCount++;
            break;
        }
    }

 
    if (frame_number == -1) {
        tlbTable->tlbMissCount++; // Increment the miss count
        // walk the contents of the page table
        for(int i = 0; i < nextPage; i++){
            if(pageTable->pageNumArr[i] == page_number){  // If the page is found in those contents
                frame_number = pageTable->frameNumArr[i]; // Extract the frame_number
                break; // Found in pageTable
            }
        }
       
        if(frame_number == -1) {  // if the page is not found in those contents
            pageTable->pageFaultCount++;   // Increment the number of page faults

            start = clock();
            readFromStore(page_number);
            cpu_time_used += (double)(clock() - start) / CLOCKS_PER_SEC;
            functionCallCount++;
            frame_number = nextFrame - 1;  // And set the frame_number to the current nextFrame index
        }
    }

    if (algo_choice == '1') {
        // Call function to insert the page number and frame number into the TLB
        tlbFIFOinsert(page_number, frame_number);
    }
    else if (algo_choice == '2'){
        tlbLRUinsert(page_number, frame_number);
    }
    else if (algo_choice == '3'){
        tlbLFU_MFUinsert(page_number, frame_number);
    }
    else if (algo_choice == '4'){
        tlbLFU_MFUinsert(page_number, frame_number);
    }


    translatedValue = dram[frame_number][offset_number];





printf("\nVirtual address: %d\t\tPhysical address: %d\t\tValue: %d", virtual_addr, (frame_number << SHIFT) | offset_number, translatedValue);
 
}

// Function to read from the backing store and bring the frame into physical memory and the page table
void readFromStore(int pageNumber)
{

    if (fseek(backing_store, pageNumber * PAGE_READ_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store\n");
    }


    if (fread(fileReadBuffer, sizeof(signed char), PAGE_READ_SIZE, backing_store) == 0) {
        fprintf(stderr, "Error reading from backing store\n");
    }

    // Load the bits into the first available frame in the physical memory 2D array
    for (int i = 0; i < PAGE_READ_SIZE; i++) {
        dram[nextFrame][i] = fileReadBuffer[i];
    }

    // Then we want to load the frame number into the page table in the next frame
    pageTable->pageNumArr[nextPage] = pageNumber;
    pageTable->frameNumArr[nextPage] = nextFrame;

    // increment the counters that track the next available frames
 
    nextFrame++;
    nextPage++;


}


void tlbFIFOinsert(int pageNumber, int frameNumber)
{
    int i;

    // If it's already in the TLB, break
    for(i = 0; i < nextTLBentry; i++){
        if(tlbTable->pageNumArr[i] == pageNumber){
            break;
        }
    }

    // if the number of entries is equal to the index
    if(i == nextTLBentry){
        if(nextTLBentry < TLB_SIZE){  // IF TLB Buffer has open positions
            tlbTable->pageNumArr[nextTLBentry] = pageNumber;    // insert the page and frame on the end
            tlbTable->frameNumArr[nextTLBentry] = frameNumber;
        }
        else{ // No room in TLB Buffer

            // Replace the last TLB entry with our new entry
            tlbTable->pageNumArr[nextTLBentry - 1] = pageNumber;
            tlbTable->frameNumArr[nextTLBentry - 1] = frameNumber;

            // Then shift up all the TLB entries by 1 so we can maintain FIFO order
            for(i = 0; i < TLB_SIZE - 1; i++){
                tlbTable->pageNumArr[i] = tlbTable->pageNumArr[i + 1];
                tlbTable->frameNumArr[i] = tlbTable->frameNumArr[i + 1];
            }
        }
    }

    // If the index is not equal to the number of entries
    else{

        for(i = i; i < nextTLBentry - 1; i++){  // iterate through up to one less than the number of entries
            // Move everything over in the arrays
            tlbTable->pageNumArr[i] = tlbTable->pageNumArr[i + 1];
            tlbTable->frameNumArr[i] = tlbTable->frameNumArr[i + 1];
        }
        if(nextTLBentry < TLB_SIZE){  // if there is still room in the array, put the page and frame on the end
             // Insert the page and frame on the end
            tlbTable->pageNumArr[nextTLBentry] = pageNumber;
            tlbTable->frameNumArr[nextTLBentry] = frameNumber;

        }
        else{  // Otherwise put the page and frame on the number of entries - 1
            tlbTable->pageNumArr[nextTLBentry - 1] = pageNumber;
            tlbTable->frameNumArr[nextTLBentry - 1] = frameNumber;
        }
    }
    if(nextTLBentry < TLB_SIZE) { // If there is still room in the arrays, increment the number of entries
        nextTLBentry++;
    }
}


// Function to insert a page number and frame number into the TLB with a LRU replacement
void tlbLRUinsert(int pageNumber, int frameNumber)
{

    bool freeSpotFound = false;
    bool alreadyThere = false;
    int replaceIndex = -1;

    // SEEK -- > Find the index to replace and increment age for all other entries
    for (int i = 0; i < TLB_SIZE; i++) {
        if ((tlbTable->pageNumArr[i] != pageNumber) && (tlbTable->pageNumArr[i] != 0)) {
            // If entry is not in TLB and not a free spot, increment its age
            tlbTable->entryAgeArr[i]++;
        }
        else if ((tlbTable->pageNumArr[i] != pageNumber) && (tlbTable->pageNumArr[i] == 0)) {
            // Available spot in TLB found
            if (!freeSpotFound) {
                replaceIndex = i;
                freeSpotFound = true;
            }
        }
        else if (tlbTable->pageNumArr[i] == pageNumber) {
            // Entry is already in TLB -- Reset its age
            if(!alreadyThere) {
                tlbTable->entryAgeArr[i] = 0;
                alreadyThere = true;
            }
        }
    }

    // REPLACE
    if (alreadyThere) {
        return;
    }
    else if (freeSpotFound) { // If we found a free spot, insert
        tlbTable->pageNumArr[replaceIndex] = pageNumber;    // Insert into free spot
        tlbTable->frameNumArr[replaceIndex] = frameNumber;
        tlbTable->entryAgeArr[replaceIndex] = 0;
    }
    else { // No more free spots -- Need to replace the oldest entry
        replaceIndex = getOldestEntry(TLB_SIZE);
        tlbTable->pageNumArr[replaceIndex] = pageNumber;    // Insert into oldest entry
        tlbTable->frameNumArr[replaceIndex] = frameNumber;
        tlbTable->entryAgeArr[replaceIndex] = 0;

    }
}

// Finds the oldest entry in TLB and returns the replacement Index
int getOldestEntry(int tlbSize) {
  int i, max, index;

  max = tlbTable->entryAgeArr[0];
  index = 0;

  for (i = 1; i < tlbSize; i++) {
    if (tlbTable->entryAgeArr[i] > max) {
       index = i;
       max = tlbTable->entryAgeArr[i];
    }
  }
  return index;
}


void tlbLFU_MFUinsert(int pageNumber, int frameNumber)
{

    bool freeSpotFound = false;
    bool alreadyThere = false;
    int replaceIndex = -1;


    for (int i = 0; i < TLB_SIZE; i++) {


        if ((tlbTable->pageNumArr[i] != pageNumber) && (tlbTable->pageNumArr[i] == 0)) {
 
            if (!freeSpotFound) {
                tlbTable->entryFrequentArr[i]++;
                replaceIndex = i;       
                freeSpotFound = true;
            }
        }
        else if (tlbTable->pageNumArr[i] == pageNumber) {
            if(!alreadyThere) {
                tlbTable->entryFrequentArr[i]++;
                alreadyThere = true;
            }
        }
    }

    // REPLACE
    if (alreadyThere) {
        return;
    }
    else if (freeSpotFound) { 
        tlbTable->pageNumArr[replaceIndex] = pageNumber;   
        tlbTable->frameNumArr[replaceIndex] = frameNumber;

        
        
    }
    else { 
    	if (algo_choice == '3')  //LFU
        	replaceIndex = getLessFrequentEntry(TLB_SIZE);
        if (algo_choice == '4') //MFU
        	replaceIndex = getMostFrequentEntry(TLB_SIZE);
        tlbTable->pageNumArr[replaceIndex] = pageNumber;    
        tlbTable->frameNumArr[replaceIndex] = frameNumber;

        tlbTable->entryFrequentArr[replaceIndex]++;
        

    }
}


int getLessFrequentEntry(int tlbSize) {
  int i, least, index;

  least = tlbTable->entryFrequentArr[0];
  index = 0;

  for (i = 1; i < tlbSize; i++) {
    if (tlbTable->entryFrequentArr[i] < least) {
       index = i;
       least = tlbTable->entryFrequentArr[i];
    }
  }
  return index;
}



int getMostFrequentEntry(int tlbSize) {
  int i, most, index;

  most = tlbTable->entryFrequentArr[0];
  index = 0;

  for (i = 1; i < tlbSize; i++) {
    if (tlbTable->entryFrequentArr[i] > most) {
       index = i;
       most = tlbTable->entryFrequentArr[i];
    }
  }
  return index;
}



// Just computes the average time complexity of accessing the backing store
double getAvgTimeInBackingStore() {
    double temp = (double) cpu_time_used / (double) functionCallCount;
    return temp * 1000000;
}
