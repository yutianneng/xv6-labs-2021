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

struct {
  struct spinlock evictlock; //置换
  struct spinlock locks[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head; //dummy node
  struct buf heads[NBUCKET];
} bcache;

int hash(uint blockno){
  return blockno%NBUCKET;
}
void
binit(void)
{
  struct buf *b;
  initlock(&bcache.evictlock,"bcache_evict");
  for (int i = 0; i < NBUCKET; i++){
    char buf[10];
    snprintf(buf,10,"bcache_%d",i);
    initlock(&bcache.locks[i],buf);
    bcache.heads[i].prev = &bcache.heads[i];
    bcache.heads[i].next = &bcache.heads[i];
  }
  //初始化block并添加到0-桶
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->lastaccesstick=ticks;
    int id=hash(b->blockno);
    b->next=bcache.heads[id].next;
    b->prev=&bcache.heads[id];

    initsleeplock(&b->lock, "buffer");
    bcache.heads[id].next->prev=b;
    bcache.heads[id].next=b;
    // printf("buffer, blockno: %d, dev: %d, disk: %d, refcnt: %d, valid: %d\n",b->blockno,b->dev,b->disk,b->refcnt,b->valid);
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id=hash(blockno);
  
  acquire(&bcache.locks[id]);

  // Is the block already cached?
  for(b = bcache.heads[id].next; b != &bcache.heads[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[id]);
      //持有锁期间可能正在io，所以用sleeplock
      acquiresleep(&b->lock);
      // printf("buffer, blockno: %d, dev: %d, disk: %d, refcnt: %d, valid: %d\n",b->blockno,b->dev,b->disk,b->refcnt,b->valid);
      return b;
    }
  }
  release(&bcache.locks[id]);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //寻找一个buffer用于置换，只有加了该锁才能改变各bucket的buffer数量

  //可能有多个访问同一block的进程到达此处，加全局锁，确保只有一个置换成功，其他的重新遍历时会命中
  acquire(&bcache.evictlock);

  acquire(&bcache.locks[id]);
  // Is the block already cached?
  for(b = bcache.heads[id].next; b != &bcache.heads[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[id]);
      release(&bcache.evictlock);
      //持有锁期间可能正在io，所以用sleeplock
      acquiresleep(&b->lock);
      // printf("buffer, blockno: %d, dev: %d, disk: %d, refcnt: %d, valid: %d\n",b->blockno,b->dev,b->disk,b->refcnt,b->valid);
      return b;
    }
  }
  release(&bcache.locks[id]);

  uint lastaccesstick=~0;
  int index=-1;
  struct buf *selectedbuf=0;
  for(int i=0;i< NBUCKET ;i++){
    int find=0;
    acquire(&bcache.locks[i]);
    for(b = bcache.heads[i].prev; b != &bcache.heads[i]; b = b->prev){
      if(b->refcnt>0){
        continue;
      }
      if(b->lastaccesstick<lastaccesstick) {
        //被选中的block的桶锁不释放，没选中的就释放掉
        //会同时加两个桶锁，但是加锁顺序一样，破坏了环路等待条件
        if(index!=-1 && index!=i){
          release(&bcache.locks[index]);
        }
        lastaccesstick=b->lastaccesstick;
        index=i;
        selectedbuf=b;
        find=1;
      }
    }
    //没找到就释放锁
    if(!find){
      release(&bcache.locks[i]);
    }
  }
  if(selectedbuf==0){
    panic("bget: no buffers");
  }

  selectedbuf->dev = dev;
  selectedbuf->blockno = blockno;
  selectedbuf->valid = 0;
  selectedbuf->refcnt = 1;
  //移除选中的buffer
  if(index!=id){
    selectedbuf->prev->next=selectedbuf->next;
    selectedbuf->next->prev=selectedbuf->prev;
  }
  release(&bcache.locks[index]);

  //添加选中的buffer到目标桶
  if(index!=id){
    acquire(&bcache.locks[id]);

    selectedbuf->next=bcache.heads[id].next;
    selectedbuf->prev=&bcache.heads[id];
    selectedbuf->next->prev=selectedbuf;
    bcache.heads[id].next=selectedbuf;

    release(&bcache.locks[id]);
  }
  

  release(&bcache.evictlock);

  acquiresleep(&selectedbuf->lock);
  return selectedbuf;
}

// Return a locked buf with the contents of the indicated block.
//返回一个加锁的buffer
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  //如果这个block不在cache中，则从磁盘上加载
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  // printf("read, blockno: %d, dev: %d, disk: %d, refcnt: %d, valid: %d\n",b->blockno,b->dev,b->disk,b->refcnt,b->valid);
  return b;
}

// Write b's contents to disk.  Must be locked.
//判断是否已经加锁，不会释放锁
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
  // printf("write, blockno: %d, dev: %d, disk: %d, refcnt: %d, valid: %d\n",b->blockno,b->dev,b->disk,b->refcnt,b->valid);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  
  int id=hash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastaccesstick=ticks;
  }
  
  release(&bcache.locks[id]);
}

void
bpin(struct buf *b) {
  int id=hash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt++;
  release(&bcache.locks[id]);
}

void
bunpin(struct buf *b) {
  int id=hash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt--;
  release(&bcache.locks[id]);
}


