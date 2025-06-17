#ifndef _LINUX_RAMFS_PERSIST_H
#define _LINUX_RAMFS_PERSIST_H

#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/path.h>

struct ramfs_persist_info {
  struct path sync_dir;
  bool   sync_bound;
  spinlock_t lock;
};

int ramfs_file_fsync(struct file *file, loff_t start, loff_t end, int datasync);

int ramfs_file_flush(struct file *file, fl_owner_t id);

int ramfs_file_release(struct inode *inode, struct file *file);

extern struct ramfs_persist_info ramfs_persist_ops;

#define RAMFS_PI(sb) ((struct ramfs_persist_info *)(sb)->s_fs_info)

#endif // _LINUX_RAMFS_PERSIST_H