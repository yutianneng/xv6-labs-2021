// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
//物理地址转页号
#define PA2PANUM(pa) (((pa)-KERNBASE)>>PGSHIFT)
#define PA2REF(pa) kcounter.refcount[PA2PANUM((uint64)(pa))]
struct {
  struct spinlock lock;
  //物理页的引用计数
  int refcount[(PHYSTOP-KERNBASE)>>PGSHIFT];
}kcounter;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

//对引用+1并返回值
int paref(uint64 pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("paref");
  acquire(&kcounter.lock);
  int count=++PA2REF(pa);
  release(&kcounter.lock);
  return count;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);

  initlock(&kcounter.lock, "kcounter");
  printf("init kcounter.lock\n");
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree"); 
  //还有其他引用，不能释放
  acquire(&kcounter.lock);
  if(--PA2REF((uint64)pa)>0){
    release(&kcounter.lock);
    return;
  }
  //避免小于0
  PA2REF((uint64)pa)=0;
  release(&kcounter.lock);
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    //重置
    PA2REF((uint64)r)=1;
  }
 
  return (void*)r;
}
//复制pa物理页，并返回新页地址
//如果pa的引用为1，则直接返回pa，
//用于copy on write
void *kcopy_and_unref(uint64 pa){
  if((pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kcopy_and_unref");

  uint64 newpa=0;
  acquire(&kcounter.lock);
  if(PA2REF(pa)<=1){
      release(&kcounter.lock);
      return (void*)pa;
  }
  if((newpa=(uint64)kalloc())==0){
    release(&kcounter.lock);
    return 0;
  }
  memmove((void*)newpa,(void*)pa,PGSIZE);
  //将原pa引用减1
  PA2REF(pa)--;
  release(&kcounter.lock);
  return (void*)newpa;
}