struct buf {
  //受bcache.locks[hash(b.blockno)]保护
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  int valid;   // has data been read from disk?

  struct sleeplock lock; //只能保护data、valid、lastaccesstick
  uint64 lastaccesstick;
  uchar data[BSIZE];
};

