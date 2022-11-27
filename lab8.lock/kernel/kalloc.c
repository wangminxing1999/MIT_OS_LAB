// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
} kmem;

void
kinit()
{
  push_off();
  int no = cpuid();
  initlock(&kmem.lock[no], "kmem");
  freerange((void*)end, (void*)PHYSTOP);
  pop_off();
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  push_off();
  int no = cpuid();
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock[no]);
  r->next = kmem.freelist[no];
  kmem.freelist[no] = r;
  release(&kmem.lock[no]);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int no = cpuid();
  struct run *r;

  acquire(&kmem.lock[no]);
  r = kmem.freelist[no];
  if(r)
    kmem.freelist[no] = r->next;
  release(&kmem.lock[no]);

  if(!r){
    for(int i=0; i<NCPU; i++){
      acquire(&kmem.lock[i]);
      if(!kmem.freelist[i]){
        release(&kmem.lock[i]);
        continue;
      }
      r = kmem.freelist[i];
      kmem.freelist[i] = r->next;
      memset((char*)r, 5, PGSIZE);
      pop_off();
      release(&kmem.lock[i]);
      return (void*)r;
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}
