/*
 *  linux/fs/file_dev.c
 *
 *  (C) 1991  Linus Torvalds
 *		------------致敬，只做了部分简化
 */
#include "fs.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
/*改自open_namei*/
int open_file(const char* pathname, int flag, int mode,
              struct m_inode** res_inode) {
  const char* basename;
  int inr, dev, namelen;
  struct m_inode *dir, *inode;
  struct buffer_head* bh;
  struct dir_entry* de;

  // 下面是先找文件夹，能否直接找文件？
  if (!(dir = dir_namei(pathname, &namelen, &basename))) return -ENOENT;
  if (!namelen) { /* special case: '/usr/' etc */  // 打开的不是文件，而是文件夹
    iput(dir);
    return -EISDIR;
  }

  bh = find_entry(&dir, basename, namelen, &de);
  // de 被指的目录项
  if (!bh) {
    /*如果打开的文件不存在，则创建它*/
    inode = new_inode(dir->i_dev);
    if (!inode) {
      iput(dir);
      return -ENOSPC;
    }
    inode->i_mode = mode;
    inode->i_dirt = 1;
    bh = add_entry(dir, basename, namelen, &de);
    if (!bh) {
      inode->i_nlinks--;  // TODO: 为什么要减，原来是1吗
      iput(inode);
      iput(dir);
      return -ENOSPC;
    }
    de->inode = inode->i_num;
    bh->b_dirt = 1;  // 该目录项所在的块，有一部分修改了即dirt
    brelse(bh);
    iput(dir);
    *res_inode = inode;
    return 0;
  }
  /*文件存在*/
  inr = de->inode;
  dev = dir->i_dev;
  brelse(bh);
  iput(dir);
  if (!(inode = iget(dev, inr))) return -EPERM;
  if (S_ISDIR(inode->i_mode) && flag != O_RDONLY) {
    iput(inode);
    return -EACCES;
  }
  inode->i_atime = CurrentTime();
  *res_inode = inode;
  return 0;
}

int file_read(struct m_inode* inode, struct file* filp, char* buf, int count) {
  int left, chars, nr;
  struct buffer_head* bh;

  if ((left = count) <= 0) return 0;
  // 应该判断位吧？有APPEND
  // if (filp->f_flags != O_RDONLY && filp->f_flags != O_RDWR)
  if (~filp->f_flags & 1) return -EACCES;

  // 能否用memcpy读而非一个一个读
  while (left) {
    if ((nr = bmap(inode, (filp->f_pos) / BLOCK_SIZE))) {
      if (!(bh = bread(nr))) break;
    } else
      bh = NULL;
    nr = filp->f_pos % BLOCK_SIZE;
    chars = MIN(BLOCK_SIZE - nr, left);
    filp->f_pos += chars;
    left -= chars;
    if (bh) {
      char* p = nr + bh->b_data;
      while (chars-- > 0) {
        buf[0] = *p;
        buf++;
        p++;
      }
      brelse(bh);
    } else {
      while (chars-- > 0) {
        buf[0] = 0;
        buf++;
      }
    }
  }
  buf[0] = 0;
  inode->i_atime = CurrentTime();
  return (count - left) ? (count - left) : -ERANGE;
}

int file_write(struct m_inode* inode, struct file* filp, char* buf, int count) {
  off_t pos;
  int block, c;
  struct buffer_head* bh;
  char* p;
  int i = 0;

  /*
   * ok, append may not work when many processes are writing at the same time
   * but so what. That way leads to madness anyway.
   */
  if (filp->f_flags != O_WRONLY && filp->f_flags != O_RDWR &&
      filp->f_flags != O_APPEND)
    return -EACCES;
  if (filp->f_flags == O_APPEND)
    pos = inode->i_size;
  else
    pos = filp->f_pos;
  while (i < count) {
    if (!(block = create_block(inode, pos / BLOCK_SIZE))) break;
    if (!(bh = bread(block))) break;
    c = pos % BLOCK_SIZE;
    p = c + bh->b_data;
    bh->b_dirt = 1;
    c = BLOCK_SIZE - c;
    if (c > count - i) c = count - i;
    pos += c;
    if (pos > inode->i_size) {
      inode->i_size = pos;
      inode->i_dirt = 1;
    }
    i += c;
    while (c-- > 0) {
      *(p++) = buf[0];
      buf++;
    }
    brelse(bh);
  }
  inode->i_mtime = CurrentTime();
  if (!(filp->f_flags == O_APPEND)) {
    filp->f_pos = pos;
    inode->i_ctime = CurrentTime();
  }

  return (i ? i : -1);
}
