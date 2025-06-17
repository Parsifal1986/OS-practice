#ifndef _UAPI_LINUX_VDSO_TASKINFO_H
#define _UAPI_LINUX_VDSO_TASKINFO_H

#include <linux/types.h>

struct vdso_taskinfo {
  __u32 pid;        /* current->pid(as an example of the information in task_struct)              */
  __u64 task_struct_ptr; /* pointer to task_struct   */
};

#endif
