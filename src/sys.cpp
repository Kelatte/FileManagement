#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#include "fs.h"
// #include<Windows.h>
#include "printfc.h"
using namespace std;

static string GetFileSize(long size) {
  float num = 1024.00;  // byte
  if (size < num) return to_string(size) + "B";
  if (size < pow(num, 2)) return to_string((size / num)) + "K";        // kb
  if (size < pow(num, 3)) return to_string(size / pow(num, 2)) + "M";  // M
  if (size < pow(num, 4)) return to_string(size / pow(num, 3)) + "G";  // G

  return to_string(size / pow(num, 4)) + "T";  // T
}
static string GetFileMode(long mode) {
  float num = 1024.00;  // byte
  if (S_ISDIR(mode)) return "目录文件";
  if (S_ISREG(mode)) return "普通文件";
  return "未知文件类型";
}
/*文件分两种，普通文件和目录文件
对于目录文件，用户使用时想要的是
使用一个目录指针，该目录指针可以得到每一条目录的名字
对于普通文件，用户使用时想要的是
使用一个文件指针，该文件指针可以得到文件指定的内容
可以得到文件名，文件大小等信息
*/
/*文件操作的系统调用，包括open，close，read，write*/

/*
 * @brief 通过文件名，访问方式，文件类型得到该文件的描述符
 * @param filename 需要访问的文件名
 * @param flag 访问方式
 * @param mode 文件类型
 * @return 文件描述符
 */
int sys_open(string filename, int flag, int mode) {
  struct m_inode* inode;
  struct file* f;
  int i, fd;

  for (fd = 0; fd < NR_OPEN; fd++) {
    if (!fileSystem->filp[fd]) break;
  }

  // 如果已打开文件个数超过上限，则出错
  if (fd >= NR_OPEN) return -EINVAL;
  f = fileSystem->filp[fd] = new file;
  if ((i = open_file(filename.c_str(), flag, mode, inode)) < 0) {
    fileSystem->filp[fd] = NULL;
    delete f;
    return i;
  }
  f->f_mode = mode;
  f->f_flags = flag;
  f->f_count = 1;
  f->f_inode = inode;
  f->f_pos = 0;
  return (fd);
}

/*
 * @brief 通过文件描述符关闭文件
 */
int sys_close(unsigned int fd) {
  struct file* filp;

  if (fd >= NR_OPEN) return -EINVAL;
  if (!(filp = fileSystem->filp[fd])) return -EINVAL;
  if (--filp->f_count) return (0);
  iput(filp->f_inode);
  delete filp;
  fileSystem->filp[fd] = NULL;
  return (0);
}

/*
 * @brief 通过文件描述符读取指定长度到buf中
 */
int sys_read(unsigned int fd, char* buf, int count) {
  struct file* file;  // 文件结构体指针，用于表示文件
  struct m_inode* inode;  // i节点结构体指针，用于表示文件的元数据信息

  // 检查文件描述符的有效性，读取长度是否合法，以及文件结构体是否存在
  if (fd >= NR_OPEN || count < 0 || !(file = fileSystem->filp[fd]))
    return -EINVAL;

  // 如果读取长度为0，直接返回0表示已经读取完文件
  if (!count) return 0;

  // 获取文件的i节点信息
  inode = file->f_inode;

  /*目前允许读目录和普通文件*/
  // 判断文件类型是否为目录或普通文件
  if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
    // 如果读取长度加上文件当前位置超过文件大小，则调整读取长度
    if (count + file->f_pos > inode->i_size)
      count = inode->i_size - file->f_pos;
    
    // 如果调整后的读取长度小于等于0，表示已经读取完文件，直接返回0
    if (count <= 0) return 0;

    // 调用文件读取函数，实际读取文件内容到缓冲区
    return file_read(inode, file, buf, count);
  }

  // 如果文件类型不是目录或普通文件，输出错误信息并返回错误码
  printf("(Read)inode->i_mode=%06o\n\r", inode->i_mode);
  return -EINVAL;
}


/*
 * @brief 将buf中指定长度的内容写入到文件描述符所指的文件中,只允许写普通文件
 */
int sys_write(unsigned int fd, char* buf, int count) {
  struct file* file;
  struct m_inode* inode;

  if (fd >= NR_OPEN || count < 0 || !(file = fileSystem->filp[fd]))
    return -EINVAL;
  if (!count) return 0;
  inode = file->f_inode;
  /*只允许写普通文件*/
  if (S_ISREG(inode->i_mode)) return file_write(inode, file, buf, count);
  printf("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
  return -EINVAL;
}

/*
 * @brief 获取给定i节点所对应文件的工作目录路径
 * @param[in] inode 指向文件i节点的指针
 * @param[out] out 存储工作目录路径的字符串
 * @return 0 表示成功，-1 表示失败
 */
int sys_get_work_dir(struct m_inode* inode, string& out) {
  int inum = inode->i_num;

  // 如果给定i节点是根目录的i节点，设置输出路径为"/"表示根目录，并返回成功
  if (inum == fileSystem->root->i_num) {
    out = "/";
    return 0;
  }

  // 初始化输出字符串为空
  out = "";
  char name[20];
  struct m_inode* fa;
  inode->i_count++;  // 之后会iput一次

  // 循环直到回溯到根目录的i节点
  while (inode->i_num != fileSystem->root->i_num) {
    // 获取当前i节点的文件名
    if (!get_name(inode, name, 20)) {
      return -1;
    }

    // 将文件名转为字符串，并在前面添加"/"
    string s(&name[0], &name[strlen(name)]);
    s.insert(0, "/");
    
    // 将当前文件名添加到输出路径的前面
    out.insert(0, s);

    // 获取当前i节点的父目录i节点
    if (!(fa = get_father(inode))) {
      return -1;
    };

    // 将当前i节点引用计数加1，因为后面会调用iput释放引用
    inode->i_count++;

    // 释放当前i节点的引用，切换到父目录i节点
    iput(inode);
    inode = fa;
  }

  return 0;
}


// ls命令 显示当前目录下所有文件
int cmd_ls(string s) {
  bool flag = false;  // 标记是否已输出表头信息
  int entries;  // 目录中的目录项总数
  int block, i;  // 当前目录项所在块号和计数器
  string out = "";
  struct buffer_head* bh;  // 缓冲区头指针，用于读取文件块数据
  struct dir_entry* de;  // 目录项结构体指针，表示目录中的一个文件或子目录
  struct m_inode* dir =
      ((s == "" || s == "-l") ? fileSystem->current : get_inode(s.c_str()));  // 获取当前目录的i节点

  // 如果目录不存在，返回目录不存在的错误码
  if (!dir) {
    return -ENOENT;
  }

  // 计算目录中的目录项总数
  entries = dir->i_size / (sizeof(struct dir_entry));

  // 获取目录的第一个数据块
  block = dir->i_zone[0];
  if (block <= 0) {
    return -EPERM;
  }

  int count = 0;
  // 读取目录项所在块的数据
  bh = bread(block);
  i = 0;
  de = (struct dir_entry*)bh->b_data;

  // 遍历目录中的所有目录项
  while (i < entries) {
    // 如果一个目录块读取完毕，则切换到下一个目录块
    if ((char*)de >= BLOCK_SIZE + bh->b_data) {
      brelse(bh);
      block = bmap(dir, i / DIR_ENTRIES_PER_BLOCK);
      bh = bread(block);
      
      // 如果下一个目录项读取失败，则跳过该目录
      if (bh == NULL || block <= 0) {
        i += DIR_ENTRIES_PER_BLOCK;
        continue;
      } else
        de = (struct dir_entry*)bh->b_data;
    }
    
    // 如果当前目录项不是".."、"."且对应的i节点存在
    if (strcmp(de->name, "..") && strcmp(de->name, ".") && de->inode != 0) {
      m_inode* inode = iget(0, de->inode);

      // 如果需要显示详细信息（-l选项）
      if (s == "-l") {
        if (!flag) {
          // 输出表头信息
          printf("%8s %13s %14s %35s\n", "mode", "size", "name",
                 "最后修改时间");
          flag = true;
        }

        // 输出详细信息，包括文件模式、文件大小、文件名和最后修改时间
        printf("%10s %13s ", GetFileMode(inode->i_mode).c_str(),
               GetFileSize(inode->i_size).c_str());
        if (S_ISDIR(inode->i_mode)) {
          printfc(FG_BLACK, BG_GREEN, "%14s", de->name);
        } else {
          printfc(FG_WHITE, "%14s", de->name);
        }
        printf(" %35s", longtoTime(inode->i_mtime));
      } else {
        // 如果不需要显示详细信息，只输出文件名或目录名
        if (S_ISDIR(inode->i_mode))
          pdirc(de->name);
        else
          pfilec(de->name);
        printf("  ");
      }

      count++;
      iput(inode);  // 释放i节点
    }
    de++;
    i++;
  }

  // 释放缓冲区头
  brelse(bh);

  // 如果目录为空，输出提示信息
  if (count <= 0) {
    pinfoc("该目录为空");
  }

  cout << endl;
  return 0;
}


// stat命令，显示文件详细信息
int cmd_stat(string path) {
  const char* basename;
  int namelen;
  struct m_inode *dir, *inode;
  struct buffer_head *bh, *dir_block;
  struct dir_entry* de;

  // 获取目录的i节点和文件名信息
  if (!(dir = dir_namei(path.c_str(), &namelen, &basename))) return -ENOENT;

  // 如果文件名为空，说明路径不存在，释放目录i节点并返回错误码
  if (!namelen) {
    iput(dir);
    return -ENOENT;
  }

  // 查找目录项并获取相应的目录块
  bh = find_entry(&dir, basename, namelen, &de);

  // 如果目录项查找失败，释放目录i节点并返回错误码
  if (!bh) {
    iput(dir);
    return -ENOENT;
  }

  // 获取文件对应的i节点
  if (!(inode = iget(dir->i_dev, de->inode))) {
    iput(dir);
    brelse(bh);
    return -EPERM;
  }

  // 输出文件详细信息，包括文件名、设备号、文件模式、硬链接数、i节点号、
  // 第一个数据块号、文件大小和最后修改时间等
  cout << "name: " << string(basename) << endl;
  cout << "dev: " << inode->i_dev << endl;
  cout << "mode: " << GetFileMode(inode->i_mode) << endl;
  cout << "nlinks: " << to_string(inode->i_nlinks) << endl;
  cout << "num: " << inode->i_num << endl;
  cout << "firstzone: " << inode->i_zone[0] << endl;
  cout << "size: " << GetFileSize(inode->i_size) << endl;
  // cout << "最后访问时间: " << longtoTime(inode->i_atime) << endl;
  cout << "最后修改时间: " << longtoTime(inode->i_mtime) << endl;
  // cout << "i节点自身最终被修改时间: " << longtoTime(inode->i_ctime) << endl;

  return 0;
}


// cd命令，移动工作目录
int cmd_cd(string path) {
  struct m_inode* dir = NULL;

  // 获取目标目录的i节点
  if ((dir = get_inode(path.c_str()))) {
    // 释放当前工作目录的i节点
    iput(fileSystem->current);
    
    // 将当前工作目录设置为目标目录
    fileSystem->current = dir;

    // 更新文件系统中的当前工作目录路径
    sys_get_work_dir(dir, fileSystem->name);
  } else {
    return -ENOENT;  // 如果目标目录不存在，返回错误码
  }

  return 0;  // 返回0表示cd命令执行成功
}

// pwd命令，输出当前路径名
int cmd_pwd() {
  struct m_inode* dir = NULL;  // 目录的i节点指针
  string pwd = "";  // 存储当前路径名的字符串

  // 调用sys_get_work_dir函数获取当前工作目录的路径
  if (sys_get_work_dir(fileSystem->current, pwd)) {
    // 如果成功获取路径，则以颜色输出当前路径名
    ppathc(pwd);
    return 0;  // 返回0表示pwd命令执行成功
  }

  return -EPERM;  // 返回错误码表示获取路径失败
}

/*cat命令，输出指定文件的所有内容*/
int cmd_cat(string path) {
  int fd, size, i;
  struct m_inode* inode;

  // 获取指定文件的i节点
  if (!(inode = get_inode(path.c_str()))) return -ENOENT;

  // 如果指定的路径为目录，执行cmd_stat命令以显示目录详细信息
  // if (inode->i_mode == S_IFDIR) return cmd_stat(path);

  // 打开文件以只读方式
  fd = sys_open(path, O_RDONLY, S_IFREG);
  if (fd < 0) {
    iput(inode);
    return fd;
  }

  // 获取文件大小
  size = inode->i_size;

  // 分配足够大的缓冲区来存储文件内容
  char* buf = new char[size + 2];

  // 读取文件内容到缓冲区
  if ((i = sys_read(fd, buf, size)) < 0) {
    sys_close(fd);
    iput(inode);
    delete[] buf;
    return i;
  }

  // 根据文件类型输出不同的信息
  if (size == 0) {
    pinfoc("文件为空\n");
  } else if (S_ISREG(inode->i_mode)) {
    // 如果是普通文件，输出文件大小和十六进制表示的内容
    pinfoc("文件大小：" + GetFileSize(size) + '\n');
    for (i = 0; i < size; ++i) {
      printf("%c", buf[i]);
    }
    printf("\n");
  } else {
    // 对于其他文件类型，以十六进制流形式输出文件内容
    pinfoc("目录大小：" + GetFileSize(size) + '\n');
    printf("下以16进制输出：\n");
    for (i = 0; i < size; ++i) {
      printf("%x ", buf[i]);
      if ((i+1) % sizeof(dir_entry) == 0) 
        printf("\n");
    }
  }

  // 关闭文件，释放i节点，释放缓冲区
  sys_close(fd);
  iput(inode);
  delete[] buf;

  return 0;  // 返回0表示cat命令执行成功
}


// vi指令，可以在文件末尾增添内容
int cmd_vi(string path) {
  int fd, size, i;
  string in = "";

  // 以追加方式打开文件
  fd = sys_open(path, O_APPEND, S_IFREG);

  // 如果打开文件失败，返回相应的错误码
  if (fd < 0) {
    return fd;
  }

  // 提示用户输入要追加的内容
  printfc(FG_YELLOW, "请输入内容：");
  cin >> in;
  getchar();  // 读取换行符

  // 将用户输入的内容追加到文件末尾
  i = sys_write(fd, (char*)in.c_str(), in.length());

  // 如果写入操作失败，关闭文件并返回错误码
  if (i < 0) {
    sys_close(fd);
    return i;
  }

  // 提示用户添加成功，并关闭文件
  psucc("\n添加成功");
  sys_close(fd);

  return 0;  // 返回0表示vi指令执行成功
}

// mkdir命令，创建一个新的文件夹（目录）
int cmd_mkdir(const char* pathname, int mode) {
  const char* basename;
  int namelen;
  struct m_inode *dir, *inode;
  struct buffer_head *bh, *dir_block;
  struct dir_entry* de;

  // 获取父目录的i节点和新目录的基本信息
  if (!(dir = dir_namei(pathname, &namelen, &basename))) return -ENOENT;

  // 如果目录名为空，说明路径不存在，释放目录i节点并返回错误码
  if (!namelen) {
    iput(dir);
    return -ENOENT;
  }

  // 判断要创建的目录是否已经存在
  bh = find_entry(&dir, basename, namelen, &de);
  if (bh) {
    brelse(bh);
    iput(dir);
    return -EEXIST;
  }

  // 开始创建新目录，首先分配新的i节点
  inode = new_inode(dir->i_dev);

  // 如果分配i节点失败，释放父目录i节点并返回错误码
  if (!inode) {
    iput(dir);
    return -ENOSPC;
  }

  // 设置新目录的基本属性，包括大小、修改时间等
  inode->i_size = 32;
  inode->i_dirt = 1;
  inode->i_mtime = inode->i_atime = CurrentTime();

  // 为新目录创建第一个数据块，包含两个子目录项 . 和 ..
  if (!(inode->i_zone[0] = new_block(inode->i_dev))) {
    iput(dir);
    inode->i_nlinks--;
    iput(inode);
    return -ENOSPC;
  }

  inode->i_dirt = 1;

  // 读取新目录的数据块，准备插入两个子目录项
  if (!(dir_block = bread(inode->i_zone[0]))) {
    iput(dir);
    free_block(inode->i_dev, inode->i_zone[0]);
    inode->i_nlinks--;
    iput(inode);
    return -1;
  }

  de = (struct dir_entry*)dir_block->b_data;

  // 加入 . 和 .. 两个子目录项
  de->inode = inode->i_num;
  strcpy(de->name, ".");
  de++;
  de->inode = dir->i_num;
  strcpy(de->name, "..");

  // 由于一个目录节点新建时，有父目录指向它，加上 . 目录项指向自己，故i_nlinks=2
  inode->i_nlinks = 2;
  dir_block->b_dirt = 1;
  brelse(dir_block);

  // 设置新目录的文件模式
  inode->i_mode = mode;
  inode->i_dirt = 1;

  // 将新目录插入到父目录中
  bh = add_entry(dir, basename, namelen, &de);

  // 如果插入子目录项失败，释放所有资源
  if (!bh) {
    iput(dir);
    free_block(inode->i_dev, inode->i_zone[0]);
    inode->i_nlinks = 0;
    iput(inode);
    return -ENOSPC;
  }

  // 插入子目录项成功，设定初始值，释放资源
  de->inode = inode->i_num;
  bh->b_dirt = 1;
  dir->i_nlinks++;
  dir->i_dirt = 1;
  iput(dir);
  iput(inode);
  brelse(bh);

  return 0;  // 返回0表示mkdir命令执行成功
}

// touch命令,创建一个普通的文件节点
int cmd_touch(const char* filename, int mode) {
  const char* basename;
  int namelen;
  struct m_inode *dir, *inode;
  struct buffer_head* bh;
  struct dir_entry* de;

  // 获取父目录的i节点和新文件的基本信息
  if (!(dir = dir_namei(filename, &namelen, &basename))) return -ENOENT;

  // 如果文件名为空，说明路径不存在，释放目录i节点并返回错误码
  if (!namelen) {
    iput(dir);
    return -ENOENT;
  }

  // 判断要创建的文件是否已经存在
  bh = find_entry(&dir, basename, namelen, &de);

  // 如果找到了文件，说明文件已存在，不能重复创建，释放资源并返回错误码
  if (bh) {
    brelse(bh);
    iput(dir);
    return -EEXIST;
  }

  // 开始创建新文件，首先分配新的i节点
  inode = new_inode(dir->i_dev);

  // 如果分配i节点失败，释放父目录i节点并返回错误码
  if (!inode) {
    iput(dir);
    return -ENOSPC;
  }

  // 设置新文件的属性，包括文件类型、修改时间等
  inode->i_mode = mode;
  inode->i_mtime = inode->i_atime = CurrentTime();
  inode->i_dirt = 1;

  // 将新文件插入到父目录中
  bh = add_entry(dir, basename, namelen, &de);

  // 如果插入文件项失败，释放所有资源
  if (!bh) {
    iput(dir);
    inode->i_nlinks = 0;
    iput(inode);
    return -ENOSPC;
  }

  // 插入文件项成功，设定初始值，释放资源
  de->inode = inode->i_num;
  bh->b_dirt = 1;
  iput(dir);
  iput(inode);
  brelse(bh);

  psucc("文件创建成功");

  return 0;  // 返回0表示touch命令执行成功
}

// rmdir命令，删除一个空的文件夹（目录）
int cmd_rmdir(const char* name) {
  const char* basename;
  int namelen;
  struct m_inode *dir, *inode;
  struct buffer_head* bh;
  struct dir_entry* de;

  // 获取父目录的i节点和要删除的目录的基本信息
  if (!(dir = dir_namei(name, &namelen, &basename))) return -ENOENT;

  // 如果目录名为空，说明路径不存在，释放目录i节点并返回错误码
  if (!namelen) {
    iput(dir);
    return -ENOENT;
  }

  // 查找要删除的目录项
  bh = find_entry(&dir, basename, namelen, &de);

  // 如果没有找到目录项，释放资源并返回错误码
  if (!bh) {
    iput(dir);
    return -ENOENT;
  }

  // 获取要删除的目录的i节点
  if (!(inode = iget(dir->i_dev, de->inode))) {
    iput(dir);
    brelse(bh);
    return -EPERM;
  }

  // 如果有其他进程正在使用该目录，释放资源并返回错误码
  if (inode->i_count > 1) {
    iput(dir);
    iput(inode);
    brelse(bh);
    return -EPERM;
  }

  // 不允许删除当前目录"."
  if (inode == dir) {
    iput(inode);
    iput(dir);
    brelse(bh);
    return -EPERM;
  }

  // 如果要删除的不是目录，而是其他类型的文件，释放资源并返回错误码
  if (!S_ISDIR(inode->i_mode)) {
    iput(inode);
    iput(dir);
    brelse(bh);
    return -ENOTDIR;
  }

  // 如果目录不为空，释放资源并返回错误码
  if (!empty_dir(inode)) {
    iput(inode);
    iput(dir);
    brelse(bh);
    return -ENOTEMPTY;
  }

  // 删除目录索引
  de->inode = 0;
  bh->b_dirt = 1;
  brelse(bh);

  // 删除目录
  inode->i_nlinks = 0;
  inode->i_dirt = 1;

  // 修改父目录信息，由于子目录的".."项被删除，所以父目录的i_nlinks--
  dir->i_nlinks--;
  dir->i_ctime = dir->i_mtime = CurrentTime();
  dir->i_dirt = 1;
  iput(dir);
  iput(inode);

  psucc("文件夹删除成功");

  return 0;  // 返回0表示rmdir命令执行成功
}

// rm命令，删除操作，删除普通文件
int cmd_rm(const char* name) {
  const char* basename;
  int namelen;
  struct m_inode *dir, *inode;
  struct buffer_head* bh;
  struct dir_entry* de;

  // 获取父目录的i节点和要删除的文件的基本信息
  if (!(dir = dir_namei(name, &namelen, &basename))) return -ENOENT;

  // 如果文件名为空，说明路径不存在，释放目录i节点并返回错误码
  if (!namelen) {
    iput(dir);
    return -ENOENT;
  }

  // 查找要删除的文件项
  bh = find_entry(&dir, basename, namelen, &de);

  // 如果没有找到文件项，释放资源并返回错误码
  if (!bh) {
    iput(dir);
    return -ENOENT;
  }

  // 获取要删除的文件的i节点
  if (!(inode = iget(dir->i_dev, de->inode))) {
    iput(dir);
    brelse(bh);
    return -ENOENT;
  }

  // 如果要删除的是目录，释放资源并返回错误码
  if (S_ISDIR(inode->i_mode)) {
    iput(inode);
    iput(dir);
    brelse(bh);
    return -EISDIR;
  }

  // 如果文件的引用数已经为0，说明出现程序bug，并修正文件i_nlinks为1
  if (!inode->i_nlinks) {
    printf("!!BUG Deleting nonexistent file (%04x:%d), %d\n", inode->i_dev,
           inode->i_num, inode->i_nlinks);
    inode->i_nlinks = 1;
  }

  // 删除文件索引
  de->inode = 0;
  bh->b_dirt = 1;
  brelse(bh);

  // 更新文件信息，减少引用数，修改修改时间，并释放资源
  inode->i_nlinks--;
  inode->i_dirt = 1;
  inode->i_ctime = CurrentTime();
  iput(inode);
  iput(dir);

  psucc("文件删除成功");

  return 0;  // 返回0表示rm命令执行成功
}


/* 保持目前的所有修改信息 */
int cmd_sync() {
  file* f;
  for (int fd = 0; fd < NR_OPEN; ++fd) {
    if ((f = fileSystem->filp[fd])) {
      iput(f->f_inode);
      delete f;
    }
  }
  realse_inode_table();
  realse_all_blocks();
  psucc("保存成功");
  return 0;
}

// exit命令，退出文件系统，将所有信息写回磁盘
int cmd_exit() {
  iput(fileSystem->current);
  iput(fileSystem->root);
  cmd_sync();
  printfc(FG_YELLOW, string("系统时间为: ") + longtoTime(CurrentTime()));
  return 0;
}

// dd命令，用于向文件追加指定数量的随机大写字母数据
int cmd_dd(const char* name) {
  int nums, fd, i;
  char* pathname = new char[strlen(name) + 1];

  // 从输入参数中解析出数据长度和文件路径
  if (sscanf(name, "%d%s", &nums, pathname) == 2) {

    // 分配存储数据的内存空间
    char* data = new char[nums];

    // 检查内存分配是否成功，如果失败则输出错误信息并返回错误码
    if (data == nullptr) {
      fprintf(stderr, "Failed to allocate memory.\n");
      return -EEXIST;
    }

    // 生成指定数量的随机大写字母数据，用于演示目的
    for (int i = 0; i < nums; ++i) {
      data[i] = 'A' + (rand() % 26);
    }

    // 打开文件，使用O_APPEND标志表示追加写入
    fd = sys_open(pathname, O_APPEND, S_IFREG);

    // 如果打开文件失败，释放内存并返回错误码
    if (fd < 0) {
      delete[] data;
      return fd;
    }

    // 将生成的数据写入文件
    i = sys_write(fd, data, nums);

    // 如果写入失败，关闭文件，释放内存，并返回错误码
    if (i < 0) {
      sys_close(fd);
      delete[] data;
      return i;
    }

    // 操作成功，输出提示信息，关闭文件，释放内存
    psucc("添加成功");
    sys_close(fd);
    delete[] data;
    return 0;

  } else {
    // 参数解析失败，返回参数错误码
    return -EINVAL;
  }

  return 0;
}

void myhint(int errorCode) {
  if (errorCode == 0) {
    // cout << "操作成功" << endl;
  } else if (errorCode == -ENOENT) {
    perrorc("路径不正确，找不到指定的路径");
  } else if (errorCode == -ENOTDIR) {
    perrorc("路径指向的不是目录文件");
  } else if (errorCode == -ENOTEMPTY) {
    perrorc("文件夹非空");
  } else if (errorCode == -EPERM) {
    perrorc("系统内部问题");
  } else if (errorCode == -EEXIST) {
    perrorc("文件已经存在");
  } else if (errorCode == -EACCES) {
    perrorc("权限不足");
  } else if (errorCode == -EINVAL) {
    perrorc("不正确的参数");
  } else if (errorCode == -ENOSPC) {
    perrorc("无法申请到资源，空间不足");
  } else if (errorCode == -EISDIR) {
    perrorc("路径指向为目录文件");
  } else {
    perrorc("未知错误");
  }
}
