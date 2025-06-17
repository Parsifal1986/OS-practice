//// filepath: /home/parsifal/repository/OperatingSysPractice/Linux-kernel/linux-5.15.178/fs/ramfs/ramfs-persist.c
#include "ramfs-persist.h"
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/mm.h>

struct ramfs_persist_info ramfs_persist_ops = {
	.sync_bound = false,
	.lock = __SPIN_LOCK_UNLOCKED(ramfs_persist_ops.lock),
};

int __ramfs_bind(const char *sync_dir_path)
{
	struct path sync_path;
	int err;

	err = kern_path(sync_dir_path, LOOKUP_FOLLOW, &sync_path);
	if (err)
		return err;

	spin_lock(&ramfs_persist_ops.lock);
	if (ramfs_persist_ops.sync_bound) {
		path_put(&ramfs_persist_ops.sync_dir);
	}
	ramfs_persist_ops.sync_dir = sync_path;
	ramfs_persist_ops.sync_bound = true;
	spin_unlock(&ramfs_persist_ops.lock);

	return 0;
}

// 供用户态使用的绑定逻辑
SYSCALL_DEFINE1(ramfs_bind, const char __user *, sync_dir_path)
{
	char *kbuf;
	int ret;

	kbuf = strndup_user(sync_dir_path, PATH_MAX);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	ret = __ramfs_bind(kbuf);
	kfree(kbuf);
	return ret;
}

/*
 * 使用页面缓存的方式读取 ramfs 文件内容，
 * 避免在 flush/close 时对 ramfs 文件使用 __kernel_read。
 */
int ramfs_read_file(struct file *filp, void **buf, size_t *size)
{
	struct address_space *mapping;
	struct inode *inode;
	size_t isize, offset = 0;
	char *kbuf;
	int err = 0;

	if (!filp || !filp->f_inode)
		return -EINVAL;
	inode = filp->f_inode;

	isize = i_size_read(inode);
	if (isize == 0) {
		*buf = NULL;
		*size = 0;
		return 0;
	}
	kbuf = kmalloc(isize, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	mapping = inode->i_mapping;
	while (offset < isize) {
		size_t page_off = offset & (PAGE_SIZE - 1);
		pgoff_t index = offset >> PAGE_SHIFT;
		size_t copy_len =
			min_t(size_t, PAGE_SIZE - page_off, isize - offset);
		struct page *page;

		page = read_mapping_page(mapping, index, NULL);
		if (IS_ERR(page)) {
			err = PTR_ERR(page);
			break;
		}
		if (!page) {
			err = -EIO;
			break;
		}
		if (PageError(page)) {
			put_page(page);
			err = -EIO;
			break;
		}

		// 拷贝数据
		memcpy(kbuf + offset, page_address(page) + page_off, copy_len);
		offset += copy_len;
		put_page(page);
	}

	if (err) {
		kfree(kbuf);
		return err;
	}
	*buf = kbuf;
	*size = isize;
	return 0;
}

/*
 * 显式同步逻辑：从 ramfs 文件读取，再写到 /tmp 目录下。
 * 避免在文件 flush/close 时执行该流程。
 */
int ramfs_do_sync(struct file *src)
{
	int err = 0;
	char path_buf[PATH_MAX], *rel_path;
	char full_path[PATH_MAX];
	void *data = NULL;
	size_t size = 0;
	struct path sync_dir_local;
	struct file *dst = NULL;
	loff_t pos = 0;

	if (!src || !src->f_inode)
		return -EINVAL;

	// 检查一下是否是 ramfs 文件
	if (strcmp(src->f_inode->i_sb->s_type->name, "ramfs") != 0)
		return 0; // 非 ramfs，直接返回

	// 获取 sync 目录
	spin_lock(&ramfs_persist_ops.lock);
	if (!ramfs_persist_ops.sync_bound) {
		spin_unlock(&ramfs_persist_ops.lock);
		return 0;
	}
	sync_dir_local = ramfs_persist_ops.sync_dir;
	path_get(&sync_dir_local);
	spin_unlock(&ramfs_persist_ops.lock);

	// 读取文件内容（使用页面缓存）
	err = ramfs_read_file(src, &data, &size);
	if (err) {
		pr_err("ramfs_sync: read failed at pos 0: %d\n", err);
		goto out_put;
	}
	pr_info("ramfs_sync: source file size = %zu\n", size);

	// 可自行决定写到什么路径，这里示例 /tmp
	rel_path =
		dentry_path_raw(src->f_path.dentry, path_buf, sizeof(path_buf));
	if (IS_ERR(rel_path)) {
		err = PTR_ERR(rel_path);
		goto out_free;
	}

	char sync_dir_buf[PATH_MAX];
	char *sync_dir_str = NULL;
	sync_dir_str = d_path(&ramfs_persist_ops.sync_dir, sync_dir_buf,
			      sizeof(sync_dir_buf));
	if (IS_ERR(sync_dir_str)) {
		err = PTR_ERR(sync_dir_str);
		pr_err("Failed to get sync_dir path: %d\n", err);
		goto out_put;
	}

	snprintf(full_path, sizeof(full_path), "%s/%s", sync_dir_str, rel_path);
	pr_info("ramfs_sync: copying to %s\n", full_path);

	// 打开目标文件
	dst = filp_open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(dst)) {
		err = PTR_ERR(dst);
		pr_err("ramfs_sync: open dst failed: %d\n", err);
		goto out_free;
	}

	spin_lock(&ramfs_persist_ops.lock);
	if (size > 0) {
		ssize_t written = kernel_write(dst, data, size, &pos);
		if (written < 0 || written != size) {
			err = (written < 0) ? written : -EIO;
			pr_err("ramfs_sync: write failed: %zd\n", written);
		} else {
			pr_info("ramfs_sync: successfully copied %zu bytes\n",
				size);
		}
		if (!err) {
			// 同步到磁盘
			err = vfs_fsync(dst, 0);
			if (err)
				pr_err("ramfs_sync: fsync failed: %d\n", err);
		}
	}
	spin_unlock(&ramfs_persist_ops.lock);

	filp_close(dst, NULL);

out_free:
	kfree(data);
out_put:
	path_put(&sync_dir_local);
	return err;
}

/*
 * 不要在 flush 时自动调用同步，以免在 close 的时机读取文件。
 * 让用户态自己显式调用 ramfs_file_flush syscall 来做同步。
 */
int ramfs_file_flush(struct file *file, fl_owner_t id)
{
	return ramfs_do_sync(file);
}

int ramfs_file_fsync(struct file *file, loff_t start, loff_t end,
			    int datasync)
{
	return ramfs_do_sync(file);
}

int ramfs_file_release(struct inode *inode, struct file *file)
{
	return ramfs_do_sync(file);
}

// 在用户态可通过 fd 调用该系统调用，显式发起同步
SYSCALL_DEFINE1(ramfs_file_flush, int, fd)
{
	int err;
	struct fd f = fdget(fd);

	if (!f.file) {
		return -EBADF;
	}

	err = ramfs_do_sync(f.file);
	fdput(f);
	return err;
}