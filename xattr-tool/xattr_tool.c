#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/xattr.h>

// 打印用法说明
void print_usage(const char *prog_name)
{
  fprintf(stderr, "用法: %s <get|set> <文件路径> <属性名> [<属性值> (仅用于 set)]\n", prog_name);
}

int main(int argc, char *argv[])
{
  // 检查参数数量
  if (argc < 4)
  {
    print_usage(argv[0]);
    return 1;
  }

  const char *action = argv[1];
  const char *filepath = argv[2];
  const char *attrname = argv[3];

  if (strcmp(action, "get") == 0)
  {
    // get 操作需要 4 个参数: 程序名, "get", 文件路径, 属性名
    if (argc != 4)
    {
      print_usage(argv[0]);
      return 1;
    }
    // 第一次调用 getxattr 获取属性值长度
    ssize_t len = getxattr(filepath, attrname, NULL, 0);
    if (len == -1)
    {
      // 如果返回 -1，说明出错或者属性不存在
      fprintf(stderr, "错误：无法获取扩展属性 \"%s\"，原因：%s\n", attrname, strerror(errno));
      return 1;
    }
    // 分配足够的缓冲区来存储属性值（len 字节）
    char *value = malloc(len + 1);
    if (value == NULL)
    {
      fprintf(stderr, "错误：内存不足，无法分配缓冲区\n");
      return 1;
    }
    // 第二次调用 getxattr 将属性值读入缓冲区
    ssize_t ret = getxattr(filepath, attrname, value, len);
    if (ret == -1)
    {
      fprintf(stderr, "错误：读取扩展属性值失败：%s\n", strerror(errno));
      free(value);
      return 1;
    }
    // 确保缓冲区以空字符结尾，以便作为字符串输出（假定属性值为文本）
    value[ret] = '\0';
    // 打印属性值
    printf("%s\n", value);
    free(value);
  }
  else if (strcmp(action, "set") == 0)
  {
    // set 操作需要 5 个参数: 程序名, "set", 文件路径, 属性名, 属性值
    if (argc != 5)
    {
      print_usage(argv[0]);
      return 1;
    }
    const char *attrvalue = argv[4];
    size_t attrvaluelen = strlen(attrvalue);
    // 调用 setxattr 设置属性值
    // flags 参数为 0：如果该属性不存在则创建，如果已存在则替换
    if (setxattr(filepath, attrname, attrvalue, attrvaluelen, 0) == -1)
    {
      fprintf(stderr, "错误：无法设置扩展属性 \"%s\"，原因：%s\n", attrname, strerror(errno));
      return 1;
    }
    printf("已设置 %s 的扩展属性 %s = \"%s\"\n", filepath, attrname, attrvalue);
  }
  else
  {
    // 未知操作，打印用法
    print_usage(argv[0]);
    return 1;
  }

  return 0;
}
