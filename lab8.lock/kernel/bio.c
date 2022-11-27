// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define HASHSIZE 13

struct {
  struct spinlock main_lock;
  struct spinlock hash_lock;
  struct spinlock lock[HASHSIZE];
  struct buf buf[NBUF];
  struct buf bucket[HASHSIZE];
  int allocated_blocks;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

void
binit(void)
{
  struct buf *b;
  
  initlock(&bcache.main_lock, "bcache");
  initlock(&bcache.hash_lock, "bcache");

  for(int i=0; i<HASHSIZE; i++){
    initlock(&bcache.lock[i], "bcache");
  }
  // Create linked list of buffers
  
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *pre = 0, *min = 0, *min_pre = 0;
  int index = blockno % HASHSIZE;
  acquire(&bcache.lock[index]);
  b = bcache.bucket[index].next;
  while(b){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  }

  acquire(&bcache.main_lock);
  if(bcache.allocated_blocks < NBUF){
    b = &bcache.buf[bcache.allocated_blocks];
    bcache.allocated_blocks ++;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    acquiresleep(&b->lock);
    b->next = bcache.bucket[index].next;
    bcache.bucket[index].next = b;
    release(&bcache.main_lock);
    release(&bcache.lock[index]);
    return b;
  }
  release(&bcache.main_lock);
  release(&bcache.lock[index]);

  acquire(&bcache.hash_lock);
  for(int i=0; i<HASHSIZE; i++){
    acquire(&bcache.lock[index]);
    uint min_time = -1;
    for(pre = &bcache.bucket[index], b = pre->next; b; pre = b, b = b->next){
      if(index == (blockno)%HASHSIZE && b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        release(&bcache.lock[index]);
        release(&bcache.hash_lock);
        acquiresleep(&b->lock);
        return b;
      }

      if(b->refcnt == 0 && b->last_access_time < min_time){
        min = b;
        min_time = b->last_access_time;
        min_pre = pre;
      }
    }
    if(min){
      min->dev = dev;
      min->blockno = blockno;
      min->valid = 0;
      min->refcnt = 1;

      if(index != (blockno)%HASHSIZE){
        min_pre->next = min->next;
        release(&bcache.lock[index]);
        acquire(&bcache.lock[(blockno)%HASHSIZE]);
        min->next = bcache.bucket[(blockno)%HASHSIZE].next;
        bcache.bucket[(blockno)%HASHSIZE].next = min;
      }
      release(&bcache.lock[(blockno)%HASHSIZE]);
      release(&bcache.hash_lock);
      acquiresleep(&min->lock);
      return min;
    }
    release(&bcache.lock[index]);
    if(++index == HASHSIZE)
      index = 0;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  int index = b->blockno % HASHSIZE;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->last_access_time = ticks;
  }
  
  release(&bcache.lock[index]);
}

void
bpin(struct buf *b) {
  int index = b->blockno % HASHSIZE;
  acquire(&bcache.lock[index]);
  b->refcnt++;
  release(&bcache.lock[index]);
}

void
bunpin(struct buf *b) {
  int index = b->blockno % HASHSIZE;
  acquire(&bcache.lock[index]);
  b->refcnt--;
  release(&bcache.lock[index]);
}


