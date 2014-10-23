#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
typedef struct _entry_t {
    uint32_t size:32;
    struct _entry_t *prev;
    struct _entry_t *next;
} entry_t;

static entry_t *head = NULL;
entry_t * tail = NULL;
void * heap_end = NULL;
size_t heap_size = 0;
#define regulate(size) size=(size&3?size+4-(size&3):size);size=(size<20?20:size)
#define vol(entry) (((entry->size>>1)<<1)+sizeof(entry_t*)+4)
#define true_size(size) (size+sizeof(entry_t*)+4)
#define get_next(entry) ((entry_t*)((void*)entry+4+((entry->size>>1)<<1)+sizeof(entry_t*)))
#define get_prev(entry) (*(entry_t**)((void*)entry-sizeof(entry_t*)))
#define back_pointer(entry) (entry_t **)((void*)entry+4+((entry->size>>1)<<1))
#define data(entry) ((void*)entry+4)
#define entry(data) ((void*)data -4)
inline void create_block(entry_t * start, unsigned size) __attribute__((always_inline));;
inline entry_t * trim_block(entry_t * block, unsigned size) __attribute__((always_inline));;
inline void prepend_block(entry_t * block) __attribute__((always_inline));;
inline entry_t * coalesce(entry_t * block) __attribute__((always_inline));;
inline entry_t * append_block(unsigned size) __attribute__((always_inline));;
inline void remove_from_list(entry_t * block) __attribute__((always_inline));;
inline entry_t * coalesce_left(entry_t * block) __attribute__((always_inline));;
inline void coalesce_right(entry_t * block) __attribute__((always_inline));;


void *calloc(size_t num, size_t size) {
    void *ptr = malloc(num * size);
	
    if (ptr)
        memset(ptr, 0x00, num * size);

    return ptr;
}


void *malloc(size_t size) {
    if (!head)
    {
	head=sbrk(2*(sizeof(entry_t)+sizeof(entry_t*)));
	if (head==(void*)-1)
            return NULL;
	heap_size = 2*(sizeof(entry_t)+sizeof(entry_t*));
	heap_end=(void*)head+heap_size;
	tail = (void*)(head+1) + sizeof(entry_t*);
	*(entry_t**)(head+1) = head;
	*(entry_t**)(tail+1) = tail;
	create_block(head,20);
	create_block(tail,20);
	head->size=1;
	tail->size=1;
	head->prev = NULL;
	head->next = tail;
	tail->prev = head;
	tail->next = NULL;
    }
    regulate(size);
    entry_t * cur = tail;
    entry_t * sel = NULL;
    while (cur){
	if ((cur->size >= size)&&(!sel || cur->size < sel->size))
	{
	    sel = cur;
	    break;
	}
	cur = cur->prev;
    }
    if (!sel) sel = append_block(size);
    if (!sel) return NULL;
    entry_t * new = trim_block(sel,size);
    sel->size++;
    remove_from_list(sel);
    if (new)
    {
	new = coalesce(new);
	prepend_block(new);
    }
    return data(sel);
}

inline void remove_from_list(entry_t * block)
{
    block->next->prev = block->prev;
    block->prev->next = block->next; 
}


inline entry_t * append_block(unsigned size)
{
    size = size < 4096*8 ? 4096*8 : size;
    size = size < (heap_size >> 5) ? (heap_size>>7)<<2 : size;
    entry_t * newBlock = sbrk(true_size(size));
    if (newBlock==(void*)-1)
        return NULL;
    heap_size += true_size(size);
    heap_end = true_size(size)+(void*)newBlock;
    create_block(newBlock,size);
    newBlock = coalesce(newBlock);
    prepend_block(newBlock);
    return newBlock;
}

//assumes regulated size
inline void create_block(entry_t * start, unsigned size)
{
    start->size=size;
    *back_pointer(start)=start;    
}

inline void prepend_block(entry_t * block)
{
    entry_t * hn = head->next;
    block->next = hn;
    hn->prev = block;
    block->prev = head;
    head->next = block;
    
}

inline entry_t * trim_block(entry_t * block, unsigned size)
{
    if (vol(block)<(true_size(size)+true_size(20))) return NULL;
    uint32_t orig_size = block->size;
    create_block(block,size);
    entry_t * new = (void*)block+true_size(size);
    create_block(new,orig_size-(orig_size%2)-vol(block));
    if(orig_size%2) block->size++;
    return new;
    
}

inline entry_t * coalesce(entry_t * block)
{
    block = coalesce_left(block);
    coalesce_right(block);
    return block;
}

inline entry_t * coalesce_left(entry_t * block)
{
    entry_t * previous = (entry_t*) get_prev(block);
    if(!(previous->size%2))
    {
	remove_from_list(previous);
	create_block(previous,block->size+vol(previous));
	block = previous;
	
    }
    return block;
}
inline void coalesce_right(entry_t * block)
{
    entry_t * next = get_next(block);
    if((void*)next<heap_end && !(next->size%2))
    {
	remove_from_list(next);
	create_block(block,block->size+vol(next));
    }
  
  
}

void free(void *ptr) {
    if (!ptr)
        return;
    
    entry_t * block = entry(ptr);
    block->size--;
    block = coalesce(block);
    prepend_block(block);
    
    
    entry_t * last = get_prev(heap_end);
    if ((!(last->size%2))&&(last->size > 64000000))
    {
	remove_from_list(last);
	heap_size -= vol(last);
	heap_end -= vol(last);
	sbrk(0-vol(last));
    }
    return;
}



void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
  
    if (!size) {
        free(ptr);
        return NULL;
    }

    entry_t * entry = entry(ptr);
    regulate(size);
    coalesce_right(entry);

    if(size <= entry->size)
    {
        entry_t * new = trim_block(entry,size);
 	if (new)
 	    prepend_block(new);
	return data(entry);
    }

    entry_t * back = coalesce_left(entry);

    if(size <= back->size)
    {
	memmove(data(back),ptr,entry->size);
        entry_t * new = trim_block(back,size);
 	if (new)
 	    prepend_block(new);
	return data(back);
    }

    void * new_data = malloc(size);
    if (!new_data)
        return NULL;
    memcpy(new_data,ptr,entry->size);
    free(data(back));
    return new_data;
}
