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

uint8 record[(PHYSTOP)/PGSIZE + 1];

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int 
cow_alloc(pagetable_t pagetable, uint64 va){
  if(va >= MAXVA)
    return -1;
  pte_t* pte;
  va = PGROUNDDOWN(va);
  pte = walk(pagetable, va, 0);
  if(pte == 0 || !(*pte & PTE_COW))
    return -1;

  uint64 pa = PTE2PA(*pte);
  if(record[pa/PGSIZE] < 2){
    *pte |= PTE_W;
    *pte &= ~PTE_COW;
    return 0;
  } else{
    char* mem;
    if((mem = kalloc()) == 0)
      return -1;
    memmove(mem,(char*)pa,PGSIZE);
    uint flags = PTE_FLAGS(*pte);
    flags |= PTE_W;
    flags &= ~PTE_COW;
    uvmunmap(pagetable, va, 1, 1);
    mappages(pagetable, va, PGSIZE, (uint64)mem, flags);
    return 0;
  }
}

void 
plus_num(uint64 pa){
  int pn = PGROUNDUP(pa) / PGSIZE;
  acquire(&kmem.lock);
  record[pn]++;
  release(&kmem.lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    plus_num((uint64)p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  acquire(&kmem.lock);
  int pn = (uint64)pa/PGSIZE;
  record[pn]--;
  if(record[pn]>0){
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  record[pn]=0;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    record[PGROUNDUP(((uint64)r))/PGSIZE] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
