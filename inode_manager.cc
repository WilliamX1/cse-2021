#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf, int size)
{
  if (id >= BLOCK_NUM) {
    printf("\tread_block: id out of range\n");
    return;
  } else memcpy(buf, this->blocks[id], size);
  return;
}

void
disk::write_block(blockid_t id, const char *buf, int size)
{
  /* 超出 BLOCK_NUM 直接返回 */
  if (id >= BLOCK_NUM) {
    printf("\tread_block: id out of range\n");
    return;
  } else memcpy(this->blocks[id], buf, size);
  return;
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */

  /* Super block 已经单独列出 */
  blockid_t block_id = 0;
  while (block_id < BLOCK_NUM && using_blocks[block_id]) block_id++;
  
  if (block_id >= BLOCK_NUM) {
    printf("\tim: alloc_block fail: no more available block");
    return BLOCK_NUM + 1;
  }

  /* 标记已经使用的 block */
  this->using_blocks[block_id] = true;

  return block_id;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  printf("\tim: free_block %d\n", id);
  this->using_blocks[id] = false;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  for (int i = 0; i < BLOCK_NUM; i++) using_blocks[i] = false;
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf, int size)
{
  d->read_block(id, buf, size);
}

void
block_manager::write_block(uint32_t id, const char *buf, int size)
{
  d->write_block(id, buf, size);
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

void
inode_manager::_alloc_inode(uint32_t type, int inum) {
    struct inode *ino = (struct inode*)malloc(sizeof(struct inode)); 
    if (ino == NULL) {
      printf("\tim: alloc_inode fail: malloc inode fail\n");
      return;
    }
    printf("\tim: alloc_inode %d\n", inum);
    ino->size = 0;
    ino->type = type;
    ino->mtime = ino->ctime = ino->atime = time(NULL);
    ino->blocks[NDIRECT] = BLOCK_NUM + 1;
    this->using_inodes[inum] = true;

    this->put_inode(inum, ino);
    free(ino);

    assert(inum != 0);
    return;
}
/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type, int iinum)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  if (iinum != -1) {
    _alloc_inode(type, iinum);
    return iinum;
  };

  uint32_t inum = 0;
  for (uint32_t i = 1; i < INODE_NUM; i++) 
    if (!using_inodes[i]) {
      inum = i;
      _alloc_inode(type, inum);
      break;
    }
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
  if (!using_inodes[inum]) {
    printf("\tim: the inode is already a freed one");
    return;
  };
  printf("\tim: free_inode %d\n", inum);

  struct inode* ino = get_inode(inum);
  
  /* 释放 block 块中内容 */
  int size = ino->size, idx = 1;
  while (idx <= NDIRECT && size > 0) {
    bm->free_block(ino->blocks[idx++]);
    size -= BLOCK_SIZE;
  };
  if (size > 0) free_inode(ino->blocks[NDIRECT]);
  ino->type = ino->size = 0;
  put_inode(inum, ino);
  free(ino);

  this->using_inodes[inum] = false;
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum, bool is_print)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  if (is_print) printf("\tim: get_inode %d\n", inum);
  if (inum < 0 || inum >= INODE_NUM) {
    if (is_print) printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);
  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    if (is_print) printf("\tim: inode not exist\n");
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

  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
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
   * and copy them to buf_out
   */

  /* 先计算所需 buf_out 长度并分配空间 */
  printf("\tim: read file inode %d\n", inum);
  if (!using_inodes[inum]) {
    printf("\tim: read_file not exist");
    return;
  }
  struct inode* ino = get_inode(inum);
  *size = ino->size;
  *buf_out = (char*)malloc(*size * sizeof(char));
  if (*buf_out == NULL) {
    printf("\tim: buf_out malloc fail\n");
    return;
  }

  int pos = 0, cnt = *size;
  while (cnt) {
    for (int i = 0; i < NDIRECT; i++) {
      blockid_t block_id = ino->blocks[i];
      char* buf = (char*)malloc(BLOCK_SIZE * sizeof(char));
      printf("cnt : %d \tblock_id: %d\n", cnt, block_id);

      if (cnt > BLOCK_SIZE) {
        bm->read_block(block_id, buf);
        memcpy(*buf_out + pos, buf, BLOCK_SIZE);
        pos += BLOCK_SIZE;
        cnt -= BLOCK_SIZE;
      } else {
        bm->read_block(block_id, buf, cnt);
        memcpy(*buf_out + pos, buf, cnt);
        pos += cnt;
        cnt = 0;
        break;
      }
      printf("\tbuf : %s\n", buf);
    };
    if (cnt) {
      ino = get_inode(ino->blocks[NDIRECT]);
    };
  };

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
  printf("\tim: write file inode %d of size: %d\n", inum, size);

  struct inode* ino = get_inode(inum);
  if (ino == NULL) {
    printf("\tim: write_file fail: inode hasn't been allocated\n");
    alloc_inode(extent_protocol::T_FILE, inum);
    ino = get_inode(inum);
  } else {
    /* free old blocks */
    // struct inode* tmp_ino = ino;
    // while (tmp_ino->size > 0) {
    //   uint32_t pos = 0;
    //   for (int i = 0; i < NDIRECT && pos < tmp_ino->size; i++) {
    //     bm->free_block(tmp_ino->blocks[i]);
    //     pos += BLOCK_SIZE;
    //   };
    //   if (pos < tmp_ino->size) tmp_ino = get_inode(tmp_ino->blocks[NDIRECT]);
    //   else break;
    // };
  }

  ino->atime = ino->ctime = ino->mtime = time(NULL);
  ino->size = size;
  ino->type = extent_protocol::T_FILE;

  ino->blocks[NDIRECT] = INODE_NUM + 1;

  uint32_t pos = 0;
  if (size) {
    for (int i = 0; i < NDIRECT; i++) {
      blockid_t block_id = ino->blocks[i] = bm->alloc_block();
      if (size > BLOCK_SIZE) {
        bm->write_block(block_id, buf + pos);
        pos += BLOCK_SIZE;
        size -= BLOCK_SIZE;
      } else {
        bm->write_block(block_id, buf + pos, size);
        pos += size;
        size = 0;
        break;
      };
    };
    put_inode(inum, ino);
    if (size > 0) {
      ino->blocks[NDIRECT] = alloc_inode(extent_protocol::T_FILE);
      put_inode(inum, ino);
      write_file(ino->blocks[NDIRECT], buf + pos, size);
    };
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
  struct inode* ino = get_inode(inum); // this->inode_table[inum]; this->get_inode(inum);

  if (ino == NULL) {
    printf("\tim: inode not exist\n");
    return;
  }

  a.type = ino->type;
  a.size = ino->size;
  a.atime = ino->atime;
  a.mtime = ino->mtime;
  a.ctime = ino->ctime;

  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  free_inode(inum);
  return;
}
