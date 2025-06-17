#ifndef __VDSO_TASKINFO_H
#define __VDSO_TASKINFO_H

#include <linux/seqlock.h>
#include <uapi/linux/vsdo_taskinfo.h>

struct vdso_taskinfo_percpu {
        seqcount_t seq;
        struct vdso_taskinfo ti;
} ____cacheline_aligned;

static struct vdso_taskinfo_percpu __percpu *vdso_ti;

#endif