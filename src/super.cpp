#include <iostream>
#include <algorithm>
#include "fs.h"
#include <cassert>
using namespace std;

static super_block* sb[NR_SUPER];
struct super_block* get_super(int dev) {
  return sb[0];
}

/*读入超级块信息*/
static struct super_block* read_super(int dev) {
  auto s = new super_block;
  buffer_head* bh;
  int i, block;

  s->s_dev = dev;
  s->s_isup = NULL;
  s->s_imount = NULL;
  s->s_time = 0;
  s->s_rd_only = 0;
  s->s_dirt = 0;
  bh = bread(1);
  *((struct d_super_block*)s) = *((struct d_super_block*)bh->b_data);

  brelse(bh);

  if (s->s_magic != SUPER_MAGIC) {
    s->s_dev = 0;
    // free_super(s);
    return NULL;
  }
  for (i = 0; i < I_MAP_SLOTS; i++) s->s_imap[i] = NULL;
  for (i = 0; i < Z_MAP_SLOTS; i++) s->s_zmap[i] = NULL;
  block = 2;
  for (i = 0; i < s->s_imap_blocks; i++) {
    s->s_imap[i] = bread(block);
    block++;
  }
  for (i = 0; i < s->s_zmap_blocks; i++) {
    s->s_zmap[i] = bread(block);
    block++;
  }
  s->s_imap[0]->b_data[0] |= 1;
  s->s_zmap[0]->b_data[0] |= 1;
  sb[0] = s;
  return s;
}

void mount_root(void) {
  int i, free;
  struct super_block* p;
  struct m_inode* mi;
  if (!(p = read_super(ROOT_DEV))) {
    printf("无法读入超级块");
    return;
  }
  if (!(mi = iget(ROOT_DEV, ROOT_INO))) {
    printf("无法读入根目录");
    return;
  }
  p->s_isup = p->s_imount = mi;
  fileSystem->root = fileSystem->current = mi;
  mi->i_count += 2;
  fileSystem->name = "/";
  free = 0;
  // bug?
  i = p->s_nzones + 1;
  //统计位图信息，给出磁盘上空闲的i节点和逻辑块
  while (--i >= 0)
    if (!get_bit(i % BLOCK_BIT, p->s_zmap[i / BLOCK_BIT]->b_data)) free++;
  printf("%d/%d free blocks\n\r", free, p->s_nzones);
  free = 0;
  i = p->s_ninodes + 1;
  while (--i >= 0)
    if (!get_bit(i % BLOCK_BIT, p->s_imap[i / BLOCK_BIT]->b_data)) free++;
  printf("%d/%d free inodes\n\r", free, p->s_ninodes);
  printf("system load!\n");
}
void initialize_block(int dev) {
  realse_inode_table();
  realse_all_blocks();
  auto ds = new d_super_block;
  memset(ds, 0, sizeof(d_super_block));
  ds->s_imap_blocks = 3; // <= I_MAP_SLOTS;
  ds->s_zmap_blocks = 8; // <= Z_MAP_SLOTS;
  ds->s_magic = SUPER_MAGIC;
  ds->s_firstdatazone = 659;
  ds->s_nzones = 62000;
  ds->s_ninodes = 20666;
  // ds->s_nzones = (1<<16) - ds->s_firstdatazone;
  // ds->s_ninodes = (ds->s_firstdatazone - 2 - ds->s_imap_blocks - ds->s_zmap_blocks) * INODES_PER_BLOCK;
  ds->s_max_size = 268966912;
  // ds->s_max_size = std::min(0ull + ds->s_nzones * BLOCK_SIZE, 0ull + BLOCK_SIZE * (7 + 512 + 512 * 512));
  ds->s_log_zone_size = 0;
  assert(ds->s_imap_blocks * 8 * BLOCK_SIZE >= ds->s_ninodes);
  assert(ds->s_zmap_blocks * 8 * BLOCK_SIZE >= ds->s_nzones);
  // 可能是要先创建inode再设置super
  // ds->s_firstdatazone = 2 + ds->s_imap_blocks + ds->s_zmap_blocks + ds->s_ninodes;
  // 写map
  int block = 2, i;
  auto buffer = new buffer_block;
  memset(buffer, 0, sizeof(buffer_block));
  for (i = 0; i < ds->s_imap_blocks; i++) {
    if (i == 0) buffer[0] = 2;
    bwrite(block, buffer);
    if (i == 0) buffer[0] = 0;
    block++;
  }
  for (i = 0; i < ds->s_zmap_blocks; i++) {
    if (i == 0) buffer[0] = 2;
    bwrite(block, buffer);
    if (i == 0) buffer[0] = 0;
    block++;
  }
  bwrite(1, (char*)ds);
  read_super(dev);
  auto inode = new_inode(dev);

  inode->i_num = ROOT_INO;
  inode->i_mode = S_IFDIR;
  inode->i_size = 32;
  inode->i_dirt = 1;
  inode->i_mtime = inode->i_atime = CurrentTime();
  inode->i_zone[0] = new_block(inode->i_dev);
  // printf("inode data block: %d\n", inode->i_zone[0]);
  buffer_head* data = bread(inode->i_zone[0]);
  auto de = (struct dir_entry*)data->b_data;
  /*加入. 和 .. 两个子目录*/
  de->inode = inode->i_num;
  strcpy(de->name, ".");
  de++;
  de->inode = inode->i_num;
  strcpy(de->name, "..");

  /*由于一个目录节点新建时，有父目录指向它，加上 .
   * 目录项指向自己，故i_nlinks=2*/
  inode->i_nlinks = 2;
  data->b_dirt = 1;
  brelse(data);
  realse_inode_table();
  realse_all_blocks();
  mount_root();
  exit(0);
}
