#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/uaccess.h>
static void mtest_listvma(void);
static struct page *my_follow_page(struct vm_area_struct *vma, unsigned long addr);
static void mtest_findpage(unsigned long addr);
static void mtest_writeval(unsigned long addr, unsigned long val);

struct proc_dir_entry *my_mtest_file;

static void mtest_listvma(void){
	struct task_struct *task = current;
	struct mm_struct *mm = task->mm;
	struct vm_area_struct *vma;
	int count = 0;

	down_read(&mm->mmap_sem);

	for(vma = mm->mmap; vma; vma = vma->vm_next){
		count++;
		printk("%d: 0x%lx 0x%lx ", count, vma->vm_start, vma->vm_end);
		
		if(vma->vm_flags & VM_READ)
			printk("r");
		else 
			printk("-");

		if(vma->vm_flags & VM_WRITE)
			printk("w");
		else 
			printk("-");

		if(vma->vm_flags & VM_EXEC)
			printk("x");
		else 
			printk("-");

		printk("\n");		
	}
	up_read(&mm->mmap_sem);
}

static struct page *my_follow_page(struct vm_area_struct *vma, unsigned long addr){
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	spinlock_t *ptl;

	struct page *page = NULL;
	struct mm_struct *mm = vma->vm_mm;
	
	// Get pgd
	pgd = pgd_offset(mm, addr);
	if(pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		goto out;
	
	// Get pud
	pud = pud_offset(pgd, addr);
	if(pud_none(*pud) || unlikely(pud_bad(*pud)))
		goto out;
	
	// Get pmd
	pmd = pmd_offset(pud, addr);
	if(pmd_none(*pmd) || unlikely(pmd_none(*pmd)))
		goto out;
	
	pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
	
	if(!pte) goto out;
	if(!pte_present(*pte)) goto unlock;
	
	page = pfn_to_page(pte_pfn(*pte));
	
	if(!page) goto unlock;
	get_page(page);
	
	unlock:
		pte_unmap_unlock(pte, ptl);

	out:
		return page;
}

static void mtest_findpage(unsigned long addr){
	struct vm_area_struct *vma;
	struct task_struct *task = current;
	struct mm_struct *mm = task->mm;
	unsigned long kernel_addr;
	struct page *page;
	
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, addr);
	page = my_follow_page(vma, addr);

	if(!page){
		printk("Translation failed.\n");
		goto out;
	}

	kernel_addr = (unsigned long) page_address(page);
	kernel_addr += (addr &~ PAGE_MASK);

	printk("vma 0x%lx -> pma 0x%lx\n", addr, kernel_addr);
	
	out:
		up_read(&mm->mmap_sem);
}

static void mtest_writeval(unsigned long addr, unsigned long val){
	struct vm_area_struct *vma;
	struct task_struct *task = current;
	struct mm_struct *mm = task->mm;
	unsigned long kernel_addr;
	struct page *page;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, addr);
	
	if(vma && addr >= vma->vm_start && (addr + sizeof(val)) < vma->vm_end){
		if(!(vma->vm_flags & VM_WRITE)){
			printk("Cannot write to 0x%lx\n", addr);
			goto out;
		}
		page = my_follow_page(vma, addr);
		
		if(!page){
			printk("Page not found\n");
			goto out;
		}
	
		kernel_addr = (unsigned long) page_address(page);
		kernel_addr += (addr &~ PAGE_MASK);
		printk("write 0x%lx to address 0x%lx\n", val, kernel_addr);
	
		*(unsigned long *)kernel_addr = val;
		put_page(page);
	} else {
		printk("no vma found for %lx\n", addr);
	}
	
	out:
		up_read(&mm->mmap_sem);
}

static ssize_t mtest_write(struct file *file, const char __user *buffer, size_t size, loff_t *data){
	char *buf;
	unsigned long input1, input2;

	buf = kmalloc((size+1), GFP_KERNEL);
	if(!buf) return -ENOMEM;

	if(copy_from_user(buf, buffer, size)){
		kfree(buf);
		return -EINVAL;
	}	
	
	if(memcmp(buf, "listvma", 7) == 0){
		mtest_listvma();
	}

	if(memcmp(buf, "findpage", 8) == 0){
		if(sscanf(buf+8, "%lx", &input1) == 1)
			mtest_findpage(input1);
	}

	if(memcmp(buf, "writeval", 8) == 0){
		if(sscanf(buf+8, "%lx %lx", &input1, &input2) == 2)
			mtest_writeval(input1, input2);
	}

	return size;
}

static const struct file_operations proc_file_operation = {
	.write = mtest_write,
};

static int __init mtest_init(void){
	my_mtest_file = proc_create("mtest", 0, NULL, &proc_file_operation);
	if(my_mtest_file == NULL){
		remove_proc_entry("mtest", NULL);
		printk(KERN_ALERT "ERROR: Cannot Initialize /proc/mtest\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "mtest Created.\n");
	return 0;
}

static void __exit mtest_exit(void){
	remove_proc_entry("mtest", NULL);
	printk(KERN_INFO "mtest Deleted.\n");
}

MODULE_LICENSE("GPL");
module_init(mtest_init);
module_exit(mtest_exit);
