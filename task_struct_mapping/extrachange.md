In core.c:

```c
#ifdef CONFIG_VDSO_TASKINFO
#include <vdso/taskinfo.h>
static void update_vdso_taskinfo(struct task_struct *p, int cpu) {
	struct vdso_taskinfo_percpu *d = per_cpu_ptr(vdso_ti, cpu);

	write_seqcount_begin(&d->seq);
	d->ti.pid = p->pid;
	d->ti.task_struct_ptr = (u64)(uintptr_t)p;
	write_seqcount_end(&d->seq);
}
#endif  /* CONFIG_VDSO_TASKINFO */
```

In vma.c:

```c
228 line: 
  else if (sym_offset == image->sym_taskinfo_page) {
		pfn = virt_to_phys(vdso_ti) >> PAGE_SHIFT;
		return vmf_insert_pfn(vma, vmf->address, pfn);
	}
```