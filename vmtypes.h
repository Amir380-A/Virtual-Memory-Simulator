#ifndef VMTYPES_H_   /* Include guard */
#define VMTYPES_H_


typedef struct vmTable_t {
    int *pageNumArr; // page number array
    int *frameNumArr; // frame number array for this
    int *entryAgeArr; // Age of each index
    int *entryFrequentArr;
    int length;
    int pageFaultCount;
    int tlbHitCount;
    int tlbMissCount;
} vmTable_t;





vmTable_t* createVMtable(int length);

// This function prints contents of the vmTable
void displayTable(vmTable_t** tableToView);



void freeVMtable(vmTable_t** table);


int** dramAllocate(int frameCount, int blockSize);

// Will free dram memory after usage
void freeDRAM(int*** dblPtrArr, int frameCount);


// 32-Bit masking function to extract page number
int getPageNumber(int mask, int value, int shift);

// 32-Bit masking function to extract page offset
int getOffset(int mask, int value);


#endif // VMTYPES_H_
