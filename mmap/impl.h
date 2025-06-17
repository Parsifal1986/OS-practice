#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/**
 * @brief 重新映射一块虚拟内存区域
 * @param addr 原始映射的内存地址，如果为 NULL 则由系统自动选择一个合适的地址
 * @param size 需要映射的大小（单位：字节）
 * @return 成功返回映射的地址，失败返回 NULL
 * @details 该函数用于重新映射一个新的虚拟内存区域。如果 addr 参数为 NULL，
 *          系统会自动选择一个合适的地址进行映射。映射的内存区域大小为 size 字节。
 *          映射失败时返回 NULL。
 */
void *mmap_remap(void *addr, size_t size)
{
  // TODO: TASK1  
  void *p;
  p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr != NULL) {
    memcpy(p, addr, size);
    msync(addr, size, MS_SYNC);
    int res = munmap(addr, size);
    if (res != 0) {
      perror("munmap");
      return NULL;
    }
  }
  return p; // 需要返回映射的地址
}

/**
 * @brief 使用 mmap 进行文件读写
 * @param filename 待操作的文件路径
 * @param offset 写入文件的偏移量（单位：字节）
 * @param content 要写入文件的内容
 * @return 成功返回 0，失败返回 -1
 * @details 该函数使用内存映射（mmap）的方式进行文件写入操作。
 *          通过 filename 指定要写入的文件，
 *          offset 指定写入的起始位置，
 *          content 指定要写入的内容。
 *          写入成功返回 0，失败返回 -1。
 */
int file_mmap_write(const char *filename, size_t offset, char *content)
{
  // TODO: TASK2
  int fd = open(filename, O_RDWR);
  if (fd == -1) {
    perror("open");
    return -1;
  }
  struct stat st;
  fstat(fd, &st);

  int size;

  if (offset + strlen(content) > st.st_size) {
    size = offset + strlen(content);
    ftruncate(fd, size);
    if (ftruncate(fd, size) < 0) {
      perror("ftruncate");
      close(fd);
      return -1;
    }
  } else {
    size = st.st_size;
  }

  
  void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    perror("mmap");
    close(fd);
    return -1;
  }

  memcpy(addr + offset, content, strlen(content));

  msync(addr, size, MS_SYNC);
  munmap(addr, size);
  close(fd);
  return 0; // 需要返回正确的执行状态
}