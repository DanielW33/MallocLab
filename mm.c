/*
 * mm.c
 *
 * Name(s): [Vid Govindappa, Daniel Williams]
 *
 * Crediting Ideas Taken from (https://danluu.com/malloc-tutorial/) no code copied, just general concepts
 *
 * Checkpoint 1: Solution Overview | Due: 2/28/21 | Submitted 2/27/21  
 * - Implemented Linked list like structure to put at the beginning of each data segment (struct metaData)
 * - Implemented Malloc: Meta data offset is 8 + 8 more bytes for alignment by 16
 * - Implemented Helper function getmetaData(void* ptr) to get ptr - offset     
 * - Implemented free: free's a block 
 * - Implemented realloc: Checks if block has space to be reallocated with size
 * TODO: Improve efficientcy by implementing helper function FindFreeBlocks to see if a block is set to free X
 * TODO: Improve efficientcy by implemeting FindFreeBlocks into malloc X
 * TODO: Improve efficientcy by modifying realloc to subdivide blocks X
 *
 * Checkpoint 2: Solution Overview | Due 3/9/21 | Submitted 3/9/21
 * - Implemented "Cache" to improve throughput
 * - Implemented helper function Find_Free_Blocks to realloc free space and improve util
 * - Current Configuration achieves score of 59/100
 * TODO: Redesign structure for checkpoint 3.
 * TODO: Reduce the amount of meta data space allocated
 * TODO: Current amount of space required for Metadata is 16 bytes per allocation.
 * IDEA: Add 1 to ptr if block is free, then it will no longer align to 16
 *
 * Checkpoint3: Solution Overview | Due 3/14/21 | Submitted 3/14/21
 * - Complete overhaul of entire project
 * - Implemented double linked list structure rather than single
 * - Implemented Free list and Used List
 * - Implemented functions to work with two lists rather than 1
 * - Increased metadata size to 32 to improve thruput by having a next and previous ptr          
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
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

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
static bool in_heap(const void*);

typedef struct{
  void* Used;       //8 Bytes
  void* Free;       //8 Bytes
} Header;

typedef struct{
  void* prev;       //8 Bytes
  void* next;       //8 Bytes
  size_t Free;      //8 Bytes
  size_t size;      //8 Bytes
}Block;

void* BaseAddr;     //8 Bytes
void* Start;        //8 Bytes
void* FreeTail;     //8 Bytes
void* UsedTail;     //8 Bytes
void* LastHeap;     //8 Bytes
size_t SearchMisses;//8 Bytes 
               //Total: 96/128 Bytes of global space used
                
/* rounds up to the nearest multiple of ALIGNMENT */

static size_t align(size_t x)
{   
  return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}
/*
 * printList:
 * Description: This prints all of the content in the current lists.
 *
 * This is used in the heap checker to represent heap invarients. It shows how each block is linked to the next.
*/ 
void printList(){
  Header* header = ((Header*) BaseAddr);
  Block* blk = header->Free;
  int Counter = 1;

  printf("---------- Free list ----------\n");
  while(blk != NULL){
    printf("%d) Block Prev: %p |Current: %p | Block next %p | Size: %zu\n",Counter , blk->prev, ((void*)blk), blk->next, blk->size);
    Counter++;
    blk = ((Block*)blk->next);
  }
  Counter = 1;
  blk = header->Used;
  printf("---------- Used list ----------\n");
  while(blk != NULL){
    printf("%d) Block Prev: %p |Current: %p | Block next %p | Size: %zu\n",Counter ,blk->prev,((void*)blk) , blk->next, blk->size);
    Counter++;
    blk = ((Block*)blk->next);
  }

}
/*
 * DestroyLists:
 * Description: Cleans up content from previous traces...
 *
 * This function is only needed because there is a chance the data structure is intact even when trace switches
 * This function function would not be needed in an ideal case.  
*/
void DestroyLists(){
  Header* header = ((Header*)BaseAddr); //Traversing the lists from start to finish.
  Block* blk = header->Used;
  Block* Current = NULL;

  while(blk != NULL){
    
    if(blk->next != NULL && blk->next < LastHeap){
      Current = blk; Current->prev = NULL; blk = blk->next; Current->next = NULL;
    }
    else{
      blk = NULL;
    }
    
  } 
 header->Used = NULL; 
  blk = ((Header*)BaseAddr)->Free;
  while(blk != NULL){
    
    if(blk->next != NULL && blk->next < LastHeap){
      Current = blk; Current->prev = NULL; blk = blk->next; Current->next = NULL;
    }
    else{
      blk = NULL;
    }
 }
  header->Free = NULL;
}
/*
 * mm_init initializes global variables needed for computation
*/
bool mm_init(void)
{
  BaseAddr = mem_sbrk(16); //Allocating header
  if(BaseAddr == NULL) return false;

  DestroyLists(); 
  LastHeap = NULL;
  Start = NULL;
  FreeTail = NULL;
  UsedTail = NULL;
  SearchMisses = 0;
  return true;
}
/*
 * AppendUsed:
 * Description: Adds a used block to the end of the Used List
 *
 * Cases to append: 
 * There are no blocks in the list
 * There are blocks in the list
 */
void AppendUsed(void* ptr){
  Block* Temp = ((Block*)ptr);

  Temp->Free = 0;
  Temp->next = NULL; Temp->prev = NULL; 
  //Assume memory is already allocated to size of ptr
  //Free block would have already been found, setting this ptr to new End or UsedTail
    
  if(UsedTail != NULL){                        //If no blocks allocated, Used Tail is null
    Block* Current_Tail = ((Block*)UsedTail);
    Block* New_Tail = ((Block*)ptr);

    Current_Tail->next = ptr;
    UsedTail = ptr;

    New_Tail->next = NULL;                    //End of list
    New_Tail->prev = ((void*)Current_Tail);   //Previous Block
    New_Tail->Free = 0;                       //Not Free

    Current_Tail->next = ((void*)New_Tail);
    New_Tail->prev = ((void*)Current_Tail);
  }
  else{                                       //This will run when there are no used blocks
    Header* header = ((Header*)BaseAddr);
    header->Used = ptr;                       //Setting first block in list and linking
    UsedTail = ptr;                           //Setting End of list.

    Block* New_Tail = ((Block*)ptr);
    New_Tail->prev = NULL;                    //Beginning of list
    New_Tail->next = NULL;                    //End of List
    New_Tail->Free = 0;                       //Not Free
  }
}
/*
 * AppendFree:
 * Description: Adds a Free block to the end of the Free list.
 *
 * Cases when appending to free:
 * There are no blocks in the list
 * There are blocks in the list
 *
 * This logic is exactly the same as AppendUsed except switched out for Free 
 */
void AppendFree(void* ptr){

  Block* Temp = ((Block*)ptr);
  Temp->Free = 1;
  Temp->next = NULL; Temp->prev = NULL;
  if(FreeTail != NULL){                       //If FreeTail has been set, Just add to end of list.  
    Block* Current_Tail = ((Block*)FreeTail); //Current_Tail is the current end
    Block* New_Tail = ((Block*)ptr);          //New_Tail is the ptr being appended to the end
    
    Current_Tail->next = ptr;                 //Current Tail->next is the new tail
    FreeTail = ptr;                           //Setting the FreeTail ptr to the New tail

    New_Tail->next = NULL;      
    New_Tail->prev = ((void*)Current_Tail);
    New_Tail->Free = 1;   

    Current_Tail->next = ((void*)New_Tail);
    New_Tail->prev = ((void*)Current_Tail);
  }
  else{                                   //This code run when the free list is empty
    Header* header = ((Header*)BaseAddr);
    header->Free = ptr;
    FreeTail = ptr;

    Block* New_Tail = ((Block*)ptr);
    New_Tail->prev = NULL;
    New_Tail->next = NULL;
    New_Tail->Free = 1;   
  } 
}
/* 
 * RemoveUsed:
 * Description: pops a used block from the used list.
 *
 * A block can be 3 things, A beginning, a middle or an end
 * Cases to look out for is 
 * Block = Beginning and end
 * Block = End
 * Block = Beginning
 * Block = in the middle
 */  
void RemoveUsed(void* ptr){  
  //Simply removing block from the list
  Block* Removed = ((Block*)ptr);

  if(Removed->next == NULL && Removed->prev == NULL){ 
    //Block is both the beginning and end of the list
    Header* header = ((Header*)BaseAddr);                 //Because the block is the beginning and the end of the list, set head to NULL
    header->Used = NULL; 
    UsedTail = NULL;
  }
  else if(Removed->next == NULL && Removed->prev != NULL){ 
    //Block is the End of the list
    Block* Previous = ((Block*)Removed->prev);            //Because the block is the end of the list, set new tail to the previous block.
    UsedTail = Removed->prev;
    Previous->next = NULL;
  }
  else if(Removed->prev == NULL && Removed->next != NULL){ 
    //Block is the beginning of the list
    Header* header = ((Header*)BaseAddr);                 //Because the block is the beginning of the list, set new head to next block.
    header->Used = Removed->next;

    Block* Next = ((Block*)Removed->next);
    Next->prev = NULL;
  }
  else if(Removed->prev != NULL && Removed->next != NULL){ 
    //Block is neither the beginning or the end of the list
    if(Removed->prev < BaseAddr){                         //Because the block is in the middle of the list, set previous block's next to Next block and
     Removed->prev = NULL;                                //set Next blocks previous to previous block  
     RemoveUsed(((void*)Removed));  
    }
    else{
      
        Block* Previous = ((Block*)Removed->prev);
        Previous->next = Removed->next;

        Block* Next = ((Block*)Removed->next);
        Next->prev = ((void*)Previous);
      
    }
  }
  Removed->Free = 1;
}
/*
 * Remove Free:  
 * Discription: pops a free block from the list.
 *
 * All logic is exactly the same as above(RemoveUsed).   
*/
void RemoveFree(void* ptr){
  Block* Removed = ((Block*)ptr);
  if(Removed->next == NULL && Removed->prev == NULL){
    Header* header = ((Header*)BaseAddr);
    header->Free = NULL;
    FreeTail = NULL;
  }
  else if(Removed->next == NULL && Removed->prev != NULL){
    Block* Previous = ((Block*)Removed->prev);
    FreeTail = Removed->prev;
    Previous->next = NULL;
  }
  else if(Removed->prev == NULL && Removed->next != NULL){
    Header* header = ((Header*)BaseAddr);
    header->Free = Removed->next;
    
    Block* Next = ((Block*)Removed->next);
    if(((void*)Next) < BaseAddr){
      Removed->next = NULL;
      RemoveFree(ptr);
    }
    else
      Next->prev = NULL;
  }
  else if(Removed->prev != NULL && Removed->next != NULL){
    if(Removed->prev  < BaseAddr){
      Removed->prev = NULL;
      RemoveFree(((void*)Removed));
    }else{
      Block* Previous = ((Block*)Removed->prev);
      Previous->next = Removed->next;

      Block* Next = ((Block*)Removed->next);
      Next->prev = ((void*)Previous);
    }
  }
  Removed->Free = 0;
}
/*
 * VerifyAdjacent:
 * Description: Verifies if adjacent block is directly adjacent. if so, combine them.
*/
void VerifyAdjacent(void* ptr){
  
  Block* Current = ((Block*)ptr);
  void* Nextblk = ((void*)(ptr + Current->size));

  if((Nextblk) < mem_sbrk(0)){                    //is ptr + Current+size greater than the end of the heap?
    
    Block* Next = ((Block*)(Nextblk));            //if so cast into block
    
    if(Next->Free == 1){                          //if block is free, combine current and next!
      size_t size = Current->size;

      Current->size = size + Next->size;
      RemoveFree(((void*)Next));
      Next->Free = 0;
    }
  }
}

/*
 * Combine_Blocks:
 * Description: This function iterates through all free blocks and combines two adjacent frees
*/ 
void Combine_Blocks(){
  Header* header = BaseAddr;
  Block* blk = NULL;
  blk = ((Block*)header->Free);

    while(blk != NULL){
  
      if(((void*)blk) + blk->size < mem_sbrk(0)){
        Block* Next = (((void*)blk) + blk->size);
        
        if(Next->Free == 1){                      //Is next free? if so combine and remove.
          size_t size = blk->size;
          blk->size = size + Next->size;
          RemoveFree(((void*)Next));
        } 
      }
      blk = ((Block*)blk->next);
    }
   
}

/*
 * Locate_Free_Block:
 * Description: iterates through all of the Free blocks and if the block is of appropriate size, allow it to be reused.
 */  
void* Locate_Free_Block(size_t size){
  
  if(SearchMisses == 20){
    Combine_Blocks();
    SearchMisses = 0;
  }

  //mm_checkheap(418);
  Header* header = ((Header*)BaseAddr);
  Block* block = ((Block*)header->Free);
   
  if(block == NULL) return mem_sbrk(0);

  while(block != NULL){                 //Iterates through list and finds a Free block of >= size of size
    VerifyAdjacent(((void*) block)); 
    
    if(block->size >= size){

      if(block->next > mem_sbrk(0)){  //Is block->next outside of heap? (precaution)
        block->next = NULL;
        block->prev = NULL;
        
        return mem_sbrk(0);
        
      }
      RemoveFree(((void*)block)); //Remove block from free list and return it to malloc
      return ((void*)block);
    }
    block = ((Block*)block->next);
  }

  SearchMisses++;
  return mem_sbrk(0);
}

/*
 * malloc
*/
void* malloc(size_t size){ 

  if(size == 0 ){
    return NULL;
  }
  else{
    
    size_t var = (size + 32);
    while(var % 16 != 0) var++;       //Ensuring alignment is rounded up to % 16
    
    if(Start == NULL){                //If first run of trace, allocate memory 
      Start = mem_sbrk(0);
      
      void* T = mem_sbrk(var);

      if(T == NULL) return NULL;
      
      Block* blk = ((Block*)T);
      blk->size = var; blk->Free = 0;
      AppendUsed(T);                  //Appending new block to used list

      return (T + 32);                //Returning position of data only (without metadata)
  
    }
    else{
      void* FoundBlock = Locate_Free_Block(var);  //Searching for block before allocating new memory

      if(FoundBlock == mem_sbrk(0)){              //if no block is found, allocate new memory
        void* T = mem_sbrk(var);
        LastHeap = mem_sbrk(0);
        
        if(T == NULL) return NULL;
        
        Block* blk = ((Block*)T);
        blk->size = var;
        blk->next = NULL; blk->prev = NULL; blk->Free = 0;
        
        AppendUsed(FoundBlock);                   //Append new block to used list
      }
      else{                                       //If code reaches here then a Free block has been found
        Block* blk = ((Block*)FoundBlock);
        blk->next = NULL; blk->prev = NULL; blk->Free = 0;

        size_t remainder = blk->size - var;

        if(remainder > 32){                                   //This is checking to see if the block can be divided into two blocks, so remainder 
          Block* NewBlock = ((Block*)(((void*)blk) + var));   //can remain free
          NewBlock->size = remainder;
          NewBlock->next = NULL; NewBlock->Free = 1; NewBlock->prev = NULL;
          blk->size = var;

          AppendFree(((void*)NewBlock)); 
        } 
                              
        AppendUsed(FoundBlock);   //Then append the original block of size var to the Usedlist
        
      }
      return (FoundBlock+32);
      
    }
       
  }
 return NULL;
}


/*
 * free
*/
void free(void* ptr)
{
  if(ptr != NULL){
    void* T = ((void*)(ptr - 32));
    Block* block = ((Block*)T);
    
    RemoveUsed(T);                                              //When freeing a block, take it off used list and put it onto free list 
    block->next = NULL; block->prev = NULL; block->Free = 0;
    AppendFree(T);
 }
  return;
}

/*
 * realloc
*/
void* realloc(void* oldptr, size_t size){
  if(oldptr == NULL){
    return malloc(size);                            //If oldptr == NULL, allocate memory for size and return the pointer                 
  }
  if(size == 0){                                    //If size == 0 free oldptr
    free(oldptr);
  }
  else{
    Block* blk = ((void*)oldptr - 32);
    if(blk->size >= (size + 32)){
      size_t var = size + 32;
      while(var % 16 != 0) var++; 
      size_t remainder = blk->size - var; 
     
      if(remainder >= 32){                                    //This is the process of subdividing a block into a smaller block,       
        Block* NewBlock = ((Block*)(((void*)(blk)) + var));   //If the remaining bytes is > 32, divide block into two and free the remaining bytes
        NewBlock->size = remainder;
        NewBlock->next = NULL; NewBlock->prev = NULL; NewBlock->Free = 1;
        blk->size = var;
        
        AppendFree(((void*)NewBlock));
      } 
      return oldptr;                                          //returning a block with enough bytes for allocation. 
    }
   else{ 
      free(oldptr);                                           //if realloc block is not large 
      void* ptr = malloc(size);
      
      memcpy(ptr , oldptr, size);
      return ptr;
    } 
  }  
  return NULL;
} 

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
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
//#ifdef DEBUG  
  printf("Check Heap: Line Number: %d\n", lineno);
  printList();
  
  void* S = BaseAddr + 16;              //By Counting all the blocks from beginning to end, it is assured that all free blocks are marked as free, 
  void* End = mem_sbrk(0);              //every free block is in the free list if the Free count from printList() equals the free count from mm_checkheap
  Block* Blk = ((Block*)((void*)(S)));  //No block overlap if this completes successfully as data must be in tact for it to run.
                                        //All pointers belong to the heap.
  size_t Free = 0;
  size_t Used = 0;

  while(S != End){
   if(Blk->Free == 0){
     Used++;
   }
   else if(Blk->Free == 1){
     Free++; 
   }
   S = ((void*)S + Blk->size);
   Blk = ((Block*)S);
   
  }
  float ratio = ((float)Free/(Free+Used)); 
 
  printf("Free: %zu | Used: %zu | Total: %zu | Free Ratio: %f\n",Free, Used, (Free+Used) ,ratio);
  
  int Counter = 0;
  S = BaseAddr + 16;
  Blk = ((Block*)((void*)S));

  while(S != End){                      //Checking for free blocks with an adjaent free block
    if(Blk->Free == 1){
      Block* Next = ((Block*)(S + Blk->size));
      
      if(Next->Free == 1){
        Counter++;
      } 
    }
    S = ((void*)S + Blk->size);
    Blk = ((Block*)S);
  } 
  printf("Escaped coalescing: %d\n", Counter);
  
//#endif /* DEBUG */
    return true;
}




