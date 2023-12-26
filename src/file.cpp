/*
 *  linux/fs/file_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */
#include "fs.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*改自open_namei*/

/*
 * @brief 打开文件并返回相应的inode
 * @param pathname 文件路径名
 * @param flag 文件打开标志
 * @param mode 文件权限
 * @param res_inode 用于返回打开的文件的inode指针
 * @return 成功返回0，失败返回相应错误码
 */
int open_file(const char* pathname, int flag, int mode,
              struct m_inode*& res_inode) {
  const char* basename;
  int inr, dev, namelen;
  struct m_inode *dir, *inode;
  struct buffer_head* bh;
  struct dir_entry* de;

  // 先根据文件路径获取父目录i节点
  if (!(dir = dir_namei(pathname, &namelen, &basename))) return -ENOENT;

  // 如果文件名为空，表示打开的是目录而不是文件
  if (!namelen) {
    iput(dir);
    return -EISDIR;
  }

  // 查找文件项
  bh = find_entry(&dir, basename, namelen, &de);

  // 如果文件项不存在，则创建新文件
  if (!bh) {
    inode = new_inode(dir->i_dev);
    if (!inode) {
      iput(dir);
      return -ENOSPC;
    }
    inode->i_mode = mode;
    inode->i_dirt = 1;
    bh = add_entry(dir, basename, namelen, &de);
    if (!bh) {
      inode->i_nlinks--;  // 如果添加目录项失败，减少文件的链接数
      iput(inode);
      iput(dir);
      return -ENOSPC;
    }
    de->inode = inode->i_num;
    bh->b_dirt = 1;  // 将目录项所在的块标记为已修改
    brelse(bh);
    iput(dir);
    res_inode = inode;
    return 0;
  }

  // 文件存在，获取文件i节点
  inr = de->inode;
  dev = dir->i_dev;
  brelse(bh);
  iput(dir);

  // 获取文件的i节点
  if (!(inode = iget(dev, inr))) return -EPERM;

  // 如果文件是目录而且要求非只读操作，则拒绝打开
  if (S_ISDIR(inode->i_mode) && flag != O_RDONLY) {
    iput(inode);
    return -EACCES;
  }

  // 更新文件访问时间
  inode->i_atime = CurrentTime();
  res_inode = inode;
  return 0;
}

/*
 * @brief 从文件中读取指定长度的内容到缓冲区中
 * @param inode 指向文件i节点的指针
 * @param filp 指向文件描述符的指针
 * @param buf 用于存储读取内容的缓冲区
 * @param count 需要读取的字节数
 * @return 返回实际读取的字节数，若出错则返回相应错误码
 */
int file_read(struct m_inode* inode, struct file* filp, char* buf, int count) {
  int left, chars, nr;
  struct buffer_head* bh;

  // 如果需要读取的字节数小于等于0，直接返回
  if ((left = count) <= 0) return 0;

  // 检查文件是否具有读权限
  if (~filp->f_flags & 1) return -EACCES;

  // 逐块读取文件内容
  while (left) {
    // 获取逻辑块号
    if ((nr = bmap(inode, (filp->f_pos) / BLOCK_SIZE))) {
      if (!(bh = bread(nr))) break;
    } else
      bh = NULL;

    // 计算在当前逻辑块中的偏移量和实际需要读取的字节数
    nr = filp->f_pos % BLOCK_SIZE;
    chars = MIN(BLOCK_SIZE - nr, left);

    // 更新文件读取位置
    filp->f_pos += chars;
    left -= chars;

    // 从缓冲块中读取数据到用户缓冲区
    if (bh) {
      char* p = nr + bh->b_data;
      while (chars-- > 0) {
        buf[0] = *p;
        buf++;
        p++;
      }
      brelse(bh);
    } else {
      // 若逻辑块号为0，说明文件大小不足，用0填充缺失的数据
      while (chars-- > 0) {
        buf[0] = 0;
        buf++;
      }
    }
  }
  buf[0] = 0;

  // 更新文件访问时间
  inode->i_atime = CurrentTime();

  // 返回实际读取的字节数，如果没有读取任何数据则返回错误码ERANGE
  return (count - left) ? (count - left) : -ERANGE;
}

/*
 * @brief 将指定长度的数据写入文件，更新文件的相关属性
 * @param inode 指向文件i节点的指针
 * @param filp 指向文件描述符的指针
 * @param buf 包含待写入数据的缓冲区
 * @param count 待写入数据的字节数
 * @return 返回实际写入的字节数，若出错则返回相应错误码
 */
int file_write(struct m_inode* inode, struct file* filp, char* buf, int count) {
  off_t pos;
  int block, c;
  struct buffer_head* bh;
  char* p;
  int i = 0;

  // 检查文件是否具有写权限，以及是否为O_APPEND模式写入
  if (filp->f_flags != O_WRONLY && filp->f_flags != O_RDWR &&
      filp->f_flags != O_APPEND)
    return -EACCES;

  // 如果是O_APPEND模式，将写入位置设置为文件末尾
  if (filp->f_flags == O_APPEND)
    pos = inode->i_size;
  else
    pos = filp->f_pos;

  // 逐块写入文件内容
  while (i < count) {
    // 获取逻辑块号并创建逻辑块
    if (!(block = create_block(inode, pos / BLOCK_SIZE))) break;

    // 读取逻辑块到缓冲区
    if (!(bh = bread(block))) break;

    // 计算在当前逻辑块中的偏移量和实际需要写入的字节数
    c = pos % BLOCK_SIZE;
    p = c + bh->b_data;
    bh->b_dirt = 1;
    c = BLOCK_SIZE - c;
    if (c > count - i) c = count - i;
    pos += c;

    // 更新文件大小
    if (pos > inode->i_size) {
      inode->i_size = pos;
      inode->i_dirt = 1;
    }

    i += c;

    // 将数据从缓冲区写入逻辑块
    while (c-- > 0) {
      *(p++) = buf[0];
      buf++;
    }

    brelse(bh);
  }

  // 更新文件修改时间
  inode->i_mtime = CurrentTime();

  // 如果不是O_APPEND模式，更新文件的写入位置和修改时间
  if (!(filp->f_flags == O_APPEND)) {
    filp->f_pos = pos;
    inode->i_ctime = CurrentTime();
  }

  return (i ? i : -1);
}
