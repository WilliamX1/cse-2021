#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

void
disk::print_status(uint32_t id)
{
  printf("\t\tBLOCK%d ", id);
  for(int i=0; i<BLOCK_SIZE; i++)
    printf("%x", blocks[id][i]);
  printf("\n");
}

// block layer -----------------------------------------

// Allocate a free disk block.
/* 从 FILEBLOCK 开始向后查找，用 'using_blocks[]' 数组记录已经分配的 block 区域。
 * 直到查找到一个还未分配的 block 区域进行使用。
 */
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  for(int i = FILEBLOCK; i < BLOCK_NUM; i++)
    if(!using_blocks[i]){
      using_blocks[i] = true;
      return i;
    }
  printf("\tim: error! alloc_block failed!\n");
  return 0;
}

/* 将对应的 block 置为 '未分配' 状态，直接给 using_blocks[]' 赋值即可。 */
void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  using_blocks[id] = false;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  
  for(uint i = 0; i < IBLOCK(INODE_NUM, sb.nblocks); i++)
    using_blocks[i] = 1;
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  assert(using_blocks[id]);
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  assert(using_blocks[id]);
  d->write_block(id, buf);
}

void
block_manager::print_status()
{
  for(int i = 0; i < BLOCK_NUM; i++)
    if(using_blocks[i])
      d->print_status(i);
}
// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  static int inum = 0;

  printf("\tim: alloc_inode %d\n", type);
  for(int i = 0; i < INODE_NUM; i++){
    inum = (inum + 1) % INODE_NUM; /* 防止超出范围 */
    inode_t *ino = get_inode(inum);
    if(!ino){ /* 该 inode 未分配则直接分配即可 */
      ino = (inode_t *)malloc(sizeof(inode_t));
      bzero(ino, sizeof(inode_t));
      ino->type = type;
      ino->atime = (unsigned int)time(NULL);
      ino->mtime = (unsigned int)time(NULL);
      ino->ctime = (unsigned int)time(NULL);
      put_inode(inum, ino);
      free(ino);
      break;
    }
    free(ino); /* 释放指针空间防止内存泄漏 */
  }

  assert(inum != 0);
  return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  printf("\tim: free_inode %d\n", inum);
  inode_t *ino = get_inode(inum);
  if(ino){
    assert(ino->type != 0); /* 检查该 inode 状态，防止二次释放空间 */
    ino->type = 0;
    
    put_inode(inum, ino);
    free(ino);
  }
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  assert(ino);

  /* 将 inode */
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

blockid_t
inode_manager::get_nth_blockid(inode_t *ino, uint32_t n)
{
  char buf[BLOCK_SIZE];
  blockid_t res;

  assert(ino);
  printf("\tim: get_nth_blockid %d\n", n);
  if (n < NDIRECT)
    res = ino->blocks[n];
  else if (n < MAXFILE){
    if (!ino->blocks[NDIRECT])
      printf("\tim: get_nth_blockid none NINDIRECT!\n");
    bm->read_block(ino->blocks[NDIRECT], buf);      
    
    res = ((blockid_t *)buf)[n - NDIRECT];
  } else{
    printf("\tim: get_nth_blockid out of range\n");
    exit(0);
  }

  return res;
}

void 
inode_manager::alloc_nth_block(inode_t *ino, uint32_t n)
{
  char buf[BLOCK_SIZE];
  assert(ino);
  printf("\tim: alloc_nth_block %d\n", n);

  if(n < NDIRECT)
    ino->blocks[n] = bm->alloc_block();
  else if(n < MAXFILE){
    if(!ino->blocks[NDIRECT]){
      printf("\tim: alloc_nth_block new NINDIRECT!\n");
      ino->blocks[NDIRECT] = bm->alloc_block();
    }
    bm->read_block(ino->blocks[NDIRECT], buf);      
    ((blockid_t *)buf)[n - NDIRECT] = bm->alloc_block();
    bm->write_block(ino->blocks[NDIRECT], buf); 
  }else{
    printf("\tim: alloc_nth_block out of range\n");
    exit(0);
  }
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  int block_num = 0;
  int remain_size = 0;
  char buf[BLOCK_SIZE];
  int i = 0;

  printf("\tim: read_file %d\n", inum);
  inode_t *ino = get_inode(inum);
  if(ino){
    *size = ino->size;
    assert((unsigned int)*size <= MAXFILE * BLOCK_SIZE);
    *buf_out = (char *)malloc(*size);

    block_num = *size / BLOCK_SIZE;
    remain_size = *size % BLOCK_SIZE;

    for(; i < block_num; i++){
      bm->read_block(get_nth_blockid(ino, i), buf);
      memcpy(*buf_out + i*BLOCK_SIZE, buf, BLOCK_SIZE);
    }
    if(remain_size){
      bm->read_block(get_nth_blockid(ino, i), buf);
      memcpy(*buf_out + i*BLOCK_SIZE, buf, remain_size);
      // printf("\tim: read_file %s %d\n", *buf_out, *size);
    }
    free(ino);
  }
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  int block_num = 0;
  int remain_size = 0;
  int old_blocks, new_blocks;
  char temp[BLOCK_SIZE];
  int i = 0;

  assert(size >= 0 && (uint32_t)size <= MAXFILE * BLOCK_SIZE);
  printf("\tim: write_file %d of size %d\n", inum, size);
  inode_t *ino = get_inode(inum);
  if(ino){
    assert((unsigned int)size <= MAXFILE * BLOCK_SIZE);
    assert(ino->size <= MAXFILE * BLOCK_SIZE);

    old_blocks = ino->size == 0? 0 : (ino->size - 1)/BLOCK_SIZE + 1;
    new_blocks = size == 0? 0 : (size - 1)/BLOCK_SIZE + 1;
    if(old_blocks < new_blocks)
      for(int j = old_blocks; j < new_blocks; j++)
        alloc_nth_block(ino, j);
    else if(old_blocks > new_blocks)
      for(int j = new_blocks; j < old_blocks; j++)
        bm->free_block(get_nth_blockid(ino, j));

    block_num = size / BLOCK_SIZE;
    remain_size = size % BLOCK_SIZE;

    for(; i < block_num; i++)
      bm->write_block(get_nth_blockid(ino, i), buf + i*BLOCK_SIZE);
    if(remain_size){
      memcpy(temp, buf + i*BLOCK_SIZE, remain_size);
      bm->write_block(get_nth_blockid(ino, i), temp);
    }

    ino->size = size;
    ino->atime = (unsigned int)time(NULL);
    ino->mtime = (unsigned int)time(NULL);
    ino->ctime = (unsigned int)time(NULL);
    put_inode(inum, ino);
    free(ino);
  }
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  printf("\tim: getattr %d\n", inum);
  inode_t *ino = get_inode(inum);
  if(!ino)
    return;
  a.type = ino->type;
  a.atime = ino->atime;
  a.mtime = ino->mtime;
  a.ctime = ino->ctime;
  a.size = ino->size;
  free(ino);
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  printf("\tim: remove_file %d\n", inum);
  inode_t *ino = get_inode(inum);
  assert(ino);

  int block_num = ino->size == 0? 0 : (ino->size - 1)/BLOCK_SIZE + 1;
  for(int i = 0; i < block_num; i++)
    bm->free_block(get_nth_blockid(ino, i));
  if(block_num > NDIRECT)
    bm->free_block(ino->blocks[NDIRECT]);
  bzero(ino, sizeof(inode_t));

  free_inode(inum);
  free(ino);
  return;
}