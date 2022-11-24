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
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];


void
kinit()
{
  for (int i = 0; i < NCPU; i++){
    char buf[10];
    snprintf(buf,10,"kmem_%d",i);
    initlock(&kmem[i].lock,buf);
  }
  
  freerange(end, (void*)PHYSTOP);
}

//均衡分配物理页，此时处于初始化阶段，由0-CPU执行，不需要考虑争用问题
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  int id=0, total=0;
  struct run *r;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    r=(struct run*)p;
    r->next = kmem[id].freelist;
    kmem[id].freelist = r;
    id=(id+1)%NCPU;
    total++;
  }
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off(); //关中断，避免破坏临界区
  int id=cpuid();
  acquire(&kmem[id].lock);

  r->next = kmem[id].freelist;
  kmem[id].freelist = r;

  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off(); //关中断，避免被切换出去，破坏临界区
  int id=cpuid();
  acquire(&kmem[id].lock);

  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  
  release(&kmem[id].lock);
  
  if(!r){
    //steal from others
    for (int i = 0; i < NCPU; i++){
      if(i!=id){
        acquire(&kmem[i].lock);
        r = kmem[i].freelist;
        if(r){
          kmem[i].freelist = r->next;
          release(&kmem[i].lock);
          break;
        }
        release(&kmem[i].lock);
      }
    }
  }
  pop_off(); //在此之前不能够被切换出去

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}

