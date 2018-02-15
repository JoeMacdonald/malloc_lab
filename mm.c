
/*
 * mm.c
 * 
 * This file is used for performing the c library functions malloc, free, 
 * realloc, and calloc. Calloc as been provided. Author Joe Macdonald wrote 
 * malloc, free, and realloc based loosely on the textbook. 
 * 
 * In init, we create a heap that we will manage by allocating or expanding 
 * as needed. Initialize to size 16384 (this is arbitrary). We also initilize a
 * global segregated list that is used to keep track of our free spaces and 
 * quickly locate a compatible block that will work with the requested data size.
 *
 * In malloc, we search our free list for a block that matches the adjusted size
 * of the space we need. Each size is rounded up to the nearest multiple of 16, 
 * then added with another 16 to provide space for the header and footer. To select
 * a block to use, begin at the earliest spot that a compatible size could exist. 
 * Traverse that list, and if no block is found, increment the index by one to 
 * search the next list. We then call the place function to optimze the block size,
 * which means splitting if necessary and adding/deleting from the list. Once the
 * list is in order, return a pointer to the space just allocated.
 *  
 * 
 * In realloc, we want to change the amount of space we use for a current block
 * that is already allocated. We call our already-written functions of malloc
 * and free. Malloc some free space, copy over old data, free old data pointer.
 * Return a pointer to the space just allocated. 
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
 * example seg list:
 *  ---  ->  ---
 * | f |    | f |  NULL
 *  ---  <-  ---
 *
 *  ---  ->  ---  ->  ---  
 * | f |    | f |    | f | ->   NULL
 *  ---  <-  ---  <-  ---  
 * 
 * NULL
 *
 *  ---  ->  ---
 * | f |    | f |  NULL
 *  ---  <-  ---
 *
 * this would contiue for 12 spaces
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

//function prototypes for helper functions
static void write_word(void *ptr, size_t val);
static size_t read_word(void *ptr);
size_t combine(size_t size, int alloc);
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
 * allocate heap
 * create prologue header
 * create prologue footer
 * create epilogue header
 * create segregated list
 * initial size of array
 */
bool mm_init(void)
{
    int index;

    //points to beginning of heap
    heap_ptr = (size_t*) heap_ptr;

    //create our array of lists
    //initialize to NULL
    for (index = 0; index < LISTSIZE; index++){
        seg_list[index] = NULL;
    }

    //create space for prologue and epilogue, plus 8 byte padding 
    if ((heap_ptr = mem_sbrk(ALIGNMENT*2)) == NULL) {
        return false;
    }

    //write 0 to beginning of heap
    //offset by 8, create header and footer of prologue
    write_word(heap_ptr, 0); 
    write_word(heap_ptr + HEADFOOT, combine(2*HEADFOOT,1));
    write_word(heap_ptr + (2*HEADFOOT), combine(2*HEADFOOT,1)); 
    heap_ptr = extendHeap(INITHEAP);

    //only use for debugging
    mm_checkheap(1);
    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    void* ptr = NULL;
    int index = 0;
    size_t asize;

    //bad request
    if (size == 0) {    
        return NULL;
    }

    //round up to smallest block size
    if (size <= 16) {
        asize = MINBLOCKSIZE;     
    }
    else { 
        //add space for header and footer
        asize = align(size + 16); //optimal alignment
    }


    //find the first bin the list that is compatible with our size
    //search for free block in segregated list
    index = placeList(asize);
    
    //if there are no spots at that index, increase and try again
    //for loop through first 10 elements of array and look for more compatible size
    if (index != 11){
        ptr = seg_list[index];
        if (ptr != NULL){
            for (int i = 0; i < 100; i++) {
                if (ptr == NULL){
                    break;
                }
                if (asize <= getSize(headerAddress(ptr))){
                    //we've found a compatible block
                    place(ptr, asize);
                    return ptr;
                }
                ptr = nextListAddPointer(ptr);
            }
        }
    }
    index += 1;
    ptr = NULL;
    while ((ptr == NULL) && (index < 12)) {
        ptr = seg_list[index];
        index++;
    }

    index--;
    if (index == 11){ //we must scan through that list
        if (ptr == NULL) { //no values in 11 either 
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

    //insert and divide block
    ptr = place(ptr, asize);

    //this is now the space of our newly allocated block
    return ptr;
}

/*
 * free a space that has been allocated
 * insert that space onto our seg list
 * change allocation bit to 0
 */
void free(void* ptr)
{
    size_t size = getSize(headerAddress(ptr));
    write_word(headerAddress(ptr), combine(size, 0));
    write_word(footerAddress(ptr), combine(size, 0));
    insertListElement(ptr, size);
    coalesce(ptr);
    return;
}

/*
 * take in old pointer and size to be changed. 
 * malloc enough space for new objetct
 * copy data from old spot into new spot
 * free old pointer, return new
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
 * commented out for submission. 
 * checks both the heap and the free list
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


/*
 * read data at a pointer address
 */
static size_t read_word(void *ptr){
    //printf("READ WORD\n");
    //printf("ptr: %p\n", ptr);
    return *(size_t *)(ptr);
}
/*
 * write data to a pointer address
 */
static void write_word(void *ptr, size_t val){
    //printf("WRITE WORD\n");
    *((size_t *)(ptr)) = val;
}

//combine the size with allocated or not for the header/footer
size_t combine(size_t size, int alloc){
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
/*
 * input header address, return size of block
 * get address of ptr
 * bitwise and with ~0x7
 * return value should be size
 */
size_t getSize(void* ptr) {
    return (size_t)(read_word(ptr) & (~0xf));
}
/*
 * input header address, return allocated or not
 * get address of ptr
 * bitsise and with 0x1
 * return value should be 1 or 0 for alloc or not
 */
size_t getAlloc(void* ptr) {
    return (size_t)(read_word(ptr) & 0x1);
}

/*
 * input pointer to block, return address of header
 */
size_t* headerAddress(void *ptr){
    return ((size_t *)(ptr) - 1);
}
/*
 * input pointer to block, return address of footer
 */
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

/*
 * we need more space, so extend the heap and create one big free block. 
 */
static void *extendHeap(size_t size) {
    size_t *p;
    size_t asize;

    //make sure new block is 16 byte aligned
    asize = align(size);
    if ((p = mem_sbrk(asize)) == NULL) {
        return NULL;
    }
    
    write_word(headerAddress(p), combine(asize, 0)); //free block header
    write_word(footerAddress(p), combine(asize, 0)); //free block footer
    write_word(headerAddress(nextBlock(p)), combine(0,1));
    insertListElement(p, asize);
    return coalesce(p);
}
/*
 * combines free blocks 
 * four cases: no coalescing needed, coalesce right, 
 * coalesce left, coalesce both
 * we will need to keep track of these within the data structure
*/
static void *coalesce(void *ptr) {
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
        write_word(headerAddress(ptr), combine(size, 0));
        write_word(footerAddress(ptr), combine(size, 0));
    }

    //coalesce left
    else if (!prev && next) {
        removeListElement(ptr);
        removeListElement(prevBlock(ptr));
        size += getSize(headerAddress(prevBlock(ptr)));
        write_word(footerAddress(ptr), combine(size,0));
        write_word(headerAddress(prevBlock(ptr)), combine(size, 0));
        ptr = prevBlock(ptr);
    }

    //coalesce both
    else {
        removeListElement(ptr);
        removeListElement(nextBlock(ptr));
        removeListElement(prevBlock(ptr));
        size += getSize(headerAddress(prevBlock(ptr)))
             + getSize(headerAddress(nextBlock(ptr)));
        write_word(headerAddress(prevBlock(ptr)), combine(size, 0));
        write_word(footerAddress(nextBlock(ptr)), combine(size, 0));
        ptr = prevBlock(ptr);
    }

    insertListElement(ptr, size);
    mm_checkheap(1);
    return ptr;
}

/*
 * value of pointer to address on seg list 
 */
size_t* prevListAddPointer(void* ptr) {
    return (*(size_t **)(ptr));
}
size_t* nextListAddPointer(void* ptr) {
    return (*(size_t **)(nListPoint(ptr)));
}

/*
 * address of free blocks predecessor and successor entries 
 */
size_t* pListPoint(void* ptr) {
    return ((size_t *)(ptr));
}
size_t* nListPoint(void* ptr) {
    return (size_t *)((ptr) + 8);
}

/*
 * places data inside a free block
 * also calculates size of free space left in block,
 * creates new block and adds it to the list if there is enough space
 */
void* place(void* ptr, size_t asize){
    size_t ptr_size = getSize(headerAddress(ptr));
    size_t leftover = ptr_size - asize;
    removeListElement(ptr);
    //depending size of leftover space, we may reallocate and place in
    //corresponding list

    //we can't split, we won't use a block this size
    if (leftover < 32) {
        write_word(headerAddress(ptr), combine(ptr_size, 1));
        write_word(footerAddress(ptr), combine(ptr_size, 1));
    }
    //we want to split the block
    else {
        write_word(headerAddress(ptr), combine(asize, 1));
        write_word(footerAddress(ptr), combine(asize, 1));
        write_word(headerAddress(nextBlock(ptr)), combine(leftover, 0));
        write_word(footerAddress(nextBlock(ptr)), combine(leftover, 0));
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
}

/*
 * adds an item to the seg list by inserting to the front
*/
static void insertListElement(void *ptr, size_t size){
    int index = 0;
    void *curr_head_ptr = NULL;

    //find list that fits with our size
    index = placeList(size);
    curr_head_ptr = seg_list[index];

    //always throw in the front of the list
    if (curr_head_ptr != NULL) { //no element in this spot
        setPointer(pListPoint(ptr), NULL);
        setPointer(nListPoint(ptr), curr_head_ptr);
        setPointer(pListPoint(curr_head_ptr), ptr);
        seg_list[index] = ptr;
    }
    else { //curr_head is equal to NULL
        setPointer(nListPoint(ptr), NULL);
        setPointer(pListPoint(ptr), NULL);
        seg_list[index] = ptr;
    }
}

/*
 * cast ptr to value, assign it into address
 * used for when simple assignment won't work
 */
static void setPointer(void *address, void *ptr){
    *(size_t *)(address) = (size_t)(ptr);
}

/*
 * scan through list, find correct index of list to take from
 * asize = is the block size we need
 */
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

/*
 * determines which block the data will go in
 * asize = is the block size we need
 */
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

/*
 * get max of two sizes (for extending heap)
 */
size_t max(size_t a, size_t b) {
    return (a > b) ? a : b;
}

//End of File
