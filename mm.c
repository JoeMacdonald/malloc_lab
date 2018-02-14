
/*
 * mm.c
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 */

/* free block example
 * -----------------------------------------
 *| header (8) | prev* | next* | footer (8) |
 * -----------------------------------------
 *
 * allocated block example
 * -----------------------------------------
 *| header (8) | data (anysize)| footer (8) |
 * -----------------------------------------
 *
 *
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want debugging output, uncomment the following. Be sure not
 * to have debugging enabled in your final submission
 */
#//define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16
#define HEADFOOT 8  //size of each header/footer
#define INITHEAP 16384 //initial heap size
#define MINBLOCKSIZE 32
#define LISTSIZE 12


void *seg_list[LISTSIZE];
void *heap_ptr;
int instructionsExecuted;

//function prototypes for helper functions
static void write_word(void *ptr, size_t val);
static size_t read_word(void *ptr);
size_t PACK(size_t size, int alloc);
static void printBlock(void* ptr);
size_t getAlloc(void* ptr);
size_t getSize(void* ptr);
static void *extendHeap(size_t size);
size_t* footerAddress(void *ptr);
size_t* headerAddress(void *ptr);
size_t* nextBlock(void* ptr);
size_t *prevBlock(void* ptr);
static void *coalesce(void *ptr);
size_t* prevListAddPointer(void* ptr);
size_t* nextListAddPointer(void* ptr);
size_t* pListPoint(void* ptr);
size_t* nListPoint(void* ptr);
void* place(void* ptr, size_t asize);
static void removeListElement(void *ptr);
static void insertListElement(void *ptr, size_t size);
static void setPointer(void *address, void *ptr);
int findList(size_t asize);
int placeList(size_t asize);
size_t max(size_t a, size_t b);



/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: return false on error, true on success.
 */
bool mm_init(void)
{
    //allocate heap
    //create prologue header
    //create prologue footer
    //create epilogue header
    //create bins/linked list objects
    //initial size of array
    instructionsExecuted = 0;
    //printf("INIT\n");
    int list;
    heap_ptr = (size_t*) heap_ptr; //points to beginning of heap

    //create our array of lists
    //initialize to NULL
    for (list = 0; list < LISTSIZE; list ++){
        seg_list[list] = NULL;
    }
    if ((heap_ptr = mem_sbrk(ALIGNMENT*2)) == NULL)
        return false;
    write_word(heap_ptr, 0); //write 0 to beginning of heap
    //offset by 8, create header and footer of prologue 
    write_word(heap_ptr + HEADFOOT, PACK(2*HEADFOOT,1));
    write_word(heap_ptr + (2*HEADFOOT), PACK(2*HEADFOOT,1)); 
    //printBlock(heap_ptr + (2*HEADFOOT));
    //mm_checkheap(1);
    heap_ptr = extendHeap(INITHEAP);
    //printf("heap_ptr: %p\n", heap_ptr);
    //printBlock(heap_ptr);
    //printf("END OF INIT\n");
    instructionsExecuted++;
    //printf("instructionsExecuted: %d\n", instructionsExecuted);
    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    //printf("MALLOC of size %lu\n", size);
    void* ptr = NULL;
    size_t asize;
    if (size == 0) {    //bad request
        return NULL;
    }

    //round up to smallest block size
    if (size <= 16) {
        asize = MINBLOCKSIZE;     
    }
    else { //add space for header and footer
        asize = align(size + 16); //optimal alignment
    }
    //find the first bin the list that is compatible with our size
    int index = 0;
    //size_t searchSize = asize;
    //search for free block in segregated list
    index = findList(asize);
    //if there are no spots at that index, increase and try again
    while ((ptr == NULL) && (index < 12)) {
        ptr = seg_list[index];
        index++;
    }
    index--;
    if (index == 11){ //we must scan through that list
        if (ptr == NULL) { //no values in 5 either 
            ptr = extendHeap(max(asize, INITHEAP));
        }
        while ((asize > getSize(headerAddress(ptr)))) {
            ptr = nextListAddPointer(ptr);
            if (ptr == NULL) {
                ptr = extendHeap(max(asize, INITHEAP));
                break;
            }
        }
        //scan through, determine if theres a spot
    }
    else if( index == -1) {
        printf("findList returned -1\n");
    }

    //insert and divide block
    ptr = place(ptr, asize);

    //this is now the space of our newly allocated block
    //printf("End of MALLOC\n");
    instructionsExecuted++;
    return ptr;
}

/*
 * free
 */
void free(void* ptr)
{
    //printf("FREE (%p of size %lu)\n", ptr, getSize(headerAddress(ptr)));
    size_t size = getSize(headerAddress(ptr));
    write_word(headerAddress(ptr), PACK(size, 0));
    write_word(footerAddress(ptr), PACK(size, 0));
    insertListElement(ptr, size);
    coalesce(ptr);
    instructionsExecuted++;
    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    void* ptr = NULL;
    //if ptr is NULL, the call is equivalent to malloc(size);
    if (oldptr == NULL){
        return malloc(size);
    }
    //if size is equal to zero, the call is equivalent to free(ptr);
    if (size == 0) {
        free(oldptr);
        return ptr;
    }

    //create a new space for data
    ptr = malloc(size);
    ptr = memcpy(ptr, oldptr, size);
    free(oldptr);

    return ptr;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ret;
    size *= nmemb;
    ret = malloc(size);
    if (ret) {
        memset(ret, 0, size);
    }
    return ret;
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
//stage 1, iterate though heap, printing each value, and make sure sizes match
    size_t size = 0;
    printf("CHECK HEAP\n");
    //printf("heap_ptr: %p\n", heap_ptr);
    heap_ptr = mem_heap_lo() + 16;
    //printf("heap_ptr: %p\n", heap_ptr);
    while (getSize(headerAddress(heap_ptr)) != 0){
        //printf("Inside loop\n");

        //size check
        size = getSize(headerAddress(heap_ptr));
        if (size != getSize(footerAddress(heap_ptr))){
            printf("Header size != footer size!\n");
        }

        //adjacent free check
        if (!getAlloc(headerAddress(heap_ptr))){
            if (!getAlloc(headerAddress(nextBlock(heap_ptr)))){
                printf("Two adjacent free blocks!\n");
            }
        }

        printBlock(heap_ptr);
        heap_ptr = heap_ptr + size;
        //printf("looping...\n");
    }
    
    printf("CHECK LIST\n");
    int index = 0;
    void* ptr = seg_list[index];

    //print out list elements
    while (index < 12) {
        printf("Block %d----\n", index);
        if (ptr == NULL) {
            index++;
            ptr = seg_list[index];
        }
        else {
            while (ptr != NULL){
                printBlock(ptr);
                if (index != placeList(getSize(headerAddress(ptr)))){
                    printf("Block in wrong list!\n");
                }
                if (getAlloc(headerAddress(ptr))){
                    printf("Block is not free!\n");
                }
                ptr = nextListAddPointer(ptr);
            }
            index++;
        }
    }
    

#endif /* DEBUG */
    return true;
}

//All helper functions
//Macros would have made things a tad easier, but 
//to be safe, they were not utilized


//reading and writing word 
static size_t read_word(void *ptr){
    //printf("READ WORD\n");
    //printf("ptr: %p\n", ptr);
    return *(size_t *)(ptr);
}

static void write_word(void *ptr, size_t val){
    //printf("WRITE WORD\n");
    *((size_t *)(ptr)) = val;
}

//combine the size with allocated or not for the header/footer
size_t PACK(size_t size, int alloc){
    return (size | alloc);
}

//printing values of block to keep track of
static void printBlock(void* ptr){
    //printf("PRINT BLOCK\n");
    //printf("Ptr argument value: %p\n", ptr);
    size_t hsize, halloc, fsize, falloc;
    hsize = getSize(headerAddress(ptr));
    halloc = getAlloc(headerAddress(ptr));
    fsize = getSize(footerAddress(ptr));
    falloc = getAlloc(footerAddress(ptr));

    //printf("GOT PAST HEADER ADDRESS, FOOTER ADDRESS, GET SIZE, GET ALLOC\n");
    if (hsize==0) {
        printf("%p: EOL\n", ptr);
        return;
    }
    printf("%p: header: [%lu:%c] footer: [%lu:%c]\n", ptr, 
       hsize, (halloc ? 'a' : 'f'), 
       fsize, (falloc ? 'a' : 'f'));
}

//input header address, return size of block
size_t getSize(void* ptr) {
    //get address of ptr
    //bitwise and with ~0x7
    //return value should be size
    //printf("GET SIZE\n");
    //printf("ptr: %p\n", ptr);
    return (size_t)(read_word(ptr) & (~0xf));
}
//input header address, return allocated or not
size_t getAlloc(void* ptr) {
    //get address of ptr
    //bitsise and with 0x1
    //return value should be 1 or 0 for alloc or not
    return (size_t)(read_word(ptr) & 0x1);
}

//input pointer to block, return address of header
size_t* headerAddress(void *ptr){
    //printf("HEADER ADDRESS\n");
    //printf("ptr: %p\n", ptr);
    return ((size_t *)(ptr) - 1);
}
//input pointer to block, return address of footer
size_t* footerAddress(void *ptr){
    //printf("FOOTER ADDRESS\n");
    return (size_t *)((ptr) + getSize(headerAddress(ptr)) - (2*HEADFOOT));
}

//returns pointer after finding headerc
size_t* nextBlock(void* ptr) {
    return (size_t *)((ptr) + getSize(headerAddress(ptr)));

}

size_t *prevBlock(void* ptr) {
    return (size_t *)((ptr) - getSize((ptr) - 16));
}

//we need more space, so extend the heap and create one big free block. 
static void *extendHeap(size_t size) {
    //printf("EXTEND HEAP\n");
    size_t *p;
    size_t asize;

    //make sure new block s 16 byte aligned
    asize = align(size);
    //printf("asize: %lu\n", asize);
    if ((p = mem_sbrk(asize)) == NULL) {
        return NULL;
    }
    //printf("asize: %lu\n", asize);
    //printf("p after mem_sbrk: %p\n", p);
    //printf("Extend Heap heap_ptr: %p\n", heap_ptr);
    //p += 1; //offset to beginning of free data
    write_word(headerAddress(p), PACK(asize, 0)); //free block header
    write_word(footerAddress(p), PACK(asize, 0)); //free block footer
    write_word(headerAddress(nextBlock(p)), PACK (0,1));
    //printf("Header address of p: %p\n", headerAddress(p));
    //printf("Footer address of p: %p\n", footerAddress(p));
    //printf("Next Block of p: %p\n", nextBlock(p));
    //mm_checkheap(1);
    insertListElement(p, asize);
    return coalesce(p);
}

//combines free blocks 
//four cases: no coalescing needed, coalesce right, 
//coalesce left, coalesce both
//we will need to keep track of these within the data structure
static void *coalesce(void *ptr) {
    //printf("COALESCE\n");

    size_t prev = getAlloc(headerAddress(prevBlock(ptr)));
    size_t next = getAlloc(headerAddress(nextBlock(ptr)));
    size_t size = getSize(headerAddress(ptr));

    //no coalescing needed
    if (prev && next) {  
        return ptr;
    }
    //coalesce right
    else if (prev && !next) {
        removeListElement(ptr);
        removeListElement(nextBlock(ptr));
        size += getSize(headerAddress(nextBlock(ptr)));
        write_word(headerAddress(ptr), PACK(size, 0));
        write_word(footerAddress(ptr), PACK(size, 0));
    }

    //coalesce left
    else if (!prev && next) {
        removeListElement(ptr);
        removeListElement(prevBlock(ptr));
        size += getSize(headerAddress(prevBlock(ptr)));
        write_word(footerAddress(ptr), PACK(size,0));
        write_word(headerAddress(prevBlock(ptr)), PACK(size, 0));
        ptr = prevBlock(ptr);
    }

    //coalesce both
    else {
        removeListElement(ptr);
        removeListElement(nextBlock(ptr));
        removeListElement(prevBlock(ptr));
        size += getSize(headerAddress(prevBlock(ptr)))
             + getSize(headerAddress(nextBlock(ptr)));
        write_word(headerAddress(prevBlock(ptr)), PACK(size, 0));
        write_word(footerAddress(nextBlock(ptr)), PACK(size, 0));
        ptr = prevBlock(ptr);
    }

    insertListElement(ptr, size);
    mm_checkheap(1);
    return ptr;
}

//value of pointer addresses on free list 
size_t* prevListAddPointer(void* ptr) {
    return (*(size_t **)(ptr));
}
size_t* nextListAddPointer(void* ptr) {
    return (*(size_t **)(nListPoint(ptr)));
}

//address of free blocks predecessor and successor entries
size_t* pListPoint(void* ptr) {
    //printf("P LIST POINT\n");
    return ((size_t *)(ptr));
}
size_t* nListPoint(void* ptr) {
    //printf("N LIST POINT\n");
    return (size_t *)((ptr) + 8);
}

//placing data inside a free block
void* place(void* ptr, size_t asize){
    //printf("PLACE\n");
    size_t ptr_size = getSize(headerAddress(ptr));
    size_t leftover = ptr_size - asize;
    removeListElement(ptr);
    //depending size of leftover space, we may reallocate and place in
    //corresponding list

    //we can't split, we won't use a block this size
    if (leftover < 32) {
        write_word(headerAddress(ptr), PACK(ptr_size, 1));
        write_word(footerAddress(ptr), PACK(ptr_size, 1));
    }
    else if (asize >=100){
        write_word(headerAddress(ptr), PACK(leftover, 0));
        write_word(footerAddress(ptr), PACK(leftover, 0));
        write_word(headerAddress(nextBlock(ptr)), PACK(asize, 1));
        write_word(footerAddress(nextBlock(ptr)), PACK(asize, 1));
        insertListElement(ptr, leftover);
        return nextBlock(ptr);
    }
    //we want to split the block
    else {
        write_word(headerAddress(ptr), PACK(asize, 1));
        write_word(footerAddress(ptr), PACK(asize, 1));
        write_word(headerAddress(nextBlock(ptr)), PACK(leftover, 0));
        write_word(footerAddress(nextBlock(ptr)), PACK(leftover, 0));
        insertListElement(nextBlock(ptr), leftover);
    }
    return ptr;
}

/*
 * removeListElement delets an item from our segregated list
 * based on a void pointer input location. Mainly swaps pointers
*/
static void removeListElement(void *ptr) {
    int index = 0;
    size_t size = getSize(headerAddress(ptr));
    //find correct list
    index = placeList(size);
    //find correct list

    if (prevListAddPointer(ptr) == NULL){
         //front of list
        if (nextListAddPointer(ptr) == NULL){
            //only element in list
            seg_list[index] = NULL;
        }
        else {
            //elements behind the list
            setPointer(pListPoint(nextListAddPointer(ptr)), NULL);
            seg_list[index] = nextListAddPointer(ptr);
        }
    }
    else {
        //not front of list
        if (nextListAddPointer(ptr) == NULL){
            //back of lsit
            setPointer(nListPoint(prevListAddPointer(ptr)), NULL);
        }
        else {
            //middle of list
            setPointer(nListPoint(prevListAddPointer(ptr)), nextListAddPointer(ptr));
            setPointer(pListPoint(nextListAddPointer(ptr)), prevListAddPointer(ptr));
        }
    }
        

    //printf("END OF REMOVE\n");
}

static void insertListElement(void *ptr, size_t size){
    //printf("INSERT LIST ELEMENT\n");
    int index = 0;
    void *curr_head_ptr = NULL;

    //find list that fits with our size
    index = placeList(size);
    //printf("index of block to be placed: %d\n", index);
    curr_head_ptr = seg_list[index];
    //always throw in the front of the list
    if (curr_head_ptr != NULL) { //no element in this spot
        //printf("curr_head is NOT equal to NULL\n");
        setPointer(pListPoint(ptr), NULL);
        setPointer(nListPoint(ptr), curr_head_ptr);
        setPointer(pListPoint(curr_head_ptr), ptr);
        seg_list[index] = ptr;
    }
    else { //curr_head is equal to NULL
        //printf("curr_head is equal to NULL\n");
        setPointer(nListPoint(ptr), NULL);
        setPointer(pListPoint(ptr), NULL);
        seg_list[index] = ptr;
    }
}

static void setPointer(void *address, void *ptr){
    //printf("SET POINTER\n");
    *(size_t *)(address) = (size_t)(ptr);
}

//scan through list, find correct index of list
//asize = is the block size we need
int findList(size_t asize){
    if (asize == 32) {
        return 0;
    }
    else if ((asize > 32) && (asize <= 64)){
        return 1;
    }
    else if ((asize > 64) && (asize <= 128)){
        return 2;
    }
    else if ((asize > 128) && (asize <= 256)){
        return 3;
    }
    else if ((asize > 256) && (asize <= 512)){
        return 4;
    }
    else if ((asize > 512) && (asize <= 1024)){
        return 5;
    }
    else if ((asize > 1024) && (asize <= 2048)){
        return 6;
    }
    else if ((asize > 2048) && (asize <= 4096)){
        return 7;
    }
    else if ((asize > 4096) && (asize <= 8192)){
        return 8;
    }
    else if ((asize > 8192) && (asize <= 16384)){
        return 9;
    }
    else if ((asize > 16384) && (asize <= 32768)){
        return 10;
    }
    else {
        return 11;
    }
}


//determines which block the data will go in
int placeList(size_t asize) {
    if (asize < 64) {
        return 0;
    }
    else if ((asize >= 64) && (asize < 128)){
        return 1;
    }
    else if ((asize >= 128) && (asize < 256)){
        return 2;
    }
    else if ((asize >= 256) && (asize < 512)){
        return 3;
    }
    else if ((asize >= 512) && (asize < 1024)){
        return 4;
    }
    else if ((asize >= 1024) && (asize < 2048)){
        return 5;
    }
    else if ((asize >= 2048) && (asize < 4096)){
        return 6;
    }
    else if ((asize >= 4096) && (asize < 8192)){
        return 7;
    }
    else if ((asize >= 8192) && (asize < 16384)){
        return 8;
    }
    else if ((asize >= 16384) && (asize < 32768)){
        return 9;
    }
    else if ((asize >= 32768) && (asize < 65536)){
        return 10;
    }
    else {
        return 11;
    }
}

//get max of two sizes
size_t max(size_t a, size_t b) {
    return (a > b) ? a : b;
}
