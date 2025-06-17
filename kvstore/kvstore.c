#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

struct kvpair {
  int key;
  int value;
  struct hlist_node node;
};

static int insert_kv(struct task_struct *task, int key, int value) {
  int index = key & 0x3FF;
  struct kvpair *new_kv;
  spin_lock(&task->kv_lock[index]);
  if (hlist_empty(&task->kv_store[index])) {
    new_kv = kmalloc(sizeof(struct kvpair), GFP_KERNEL);
    if (!new_kv) {
	    spin_unlock(&task->kv_lock[index]);
	    return -1;
    }
    new_kv->key = key;
    new_kv->value = value;
    hlist_add_head(&new_kv->node, &task->kv_store[index]);
  } else {
    struct hlist_node *pos;;
    hlist_for_each(pos, &task->kv_store[index]) {
      struct kvpair *kv = hlist_entry(pos, struct kvpair, node);
      if (kv->key == key) {
	      kv->value = value;
	      spin_unlock(&task->kv_lock[index]);
	      return 0;
      }
      new_kv = kmalloc(sizeof(struct kvpair), GFP_KERNEL);
      if (!new_kv) {
	      spin_unlock(&task->kv_lock[index]);
	      return -1;
      }
      new_kv->key = key;
      new_kv->value = value;
      hlist_add_head(&new_kv->node, &task->kv_store[index]);
    }
  }
  spin_unlock(&task->kv_lock[index]);
  return 0;
}

static int query_kv(struct task_struct *task, int key) {
	int index = key & 0x3FF;
	spin_lock(&task->kv_lock[index]);
	if (hlist_empty(&task->kv_store[index])) {
		spin_unlock(&task->kv_lock[index]);
		return -1;
	} else {
		struct hlist_node *pos;
		hlist_for_each (pos, &task->kv_store[index]) {
			struct kvpair *kv = hlist_entry(pos, struct kvpair, node);
			if (kv->key == key) {
				spin_unlock(&task->kv_lock[index]);
				return kv->value;
			}
		}
  }
  spin_unlock(&task->kv_lock[index]);
  return -2;
}

asmlinkage long __x64_sys_insert_kv(const struct pt_regs *regs)
{
	struct task_struct *task = current;
	int key = (int)regs->di;
	int value = (int)regs->si;

	return insert_kv(task, key, value);
}

asmlinkage long __x64_sys_query_kv(const struct pt_regs *regs)
{
	struct task_struct *task = current;
	int key = (int)regs->di;

	return query_kv(task, key);
}

static int __init kvstore_init(void)
{
	printk(KERN_INFO "KVStore module loaded\n");
  return 0;
}

static void __exit kvstore_exit(void) {
  printk(KERN_INFO "Exiting kvstore module\n");
}

module_init(kvstore_init);
module_exit(kvstore_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chuxi Wu");
MODULE_DESCRIPTION("Kernel Module for Key-Value Store");