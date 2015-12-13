#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/setup.h>
#include <asm/types.h>
#include <asm/page.h>
#include <asm/mach/arch.h>
#include <asm/cacheflush.h>
#include <linux/highmem.h>
#include <uapi/asm/setup.h>

#include <linux/module.h>

MODULE_LICENSE("GPL");

#ifndef MY_MEM_SIZE
#define MY_MEM_SIZE	(890*1024*1024)
#endif

#define IDME_ATAG_SIZE  8736 
#define ATAG_IDME_SIZE 8736
struct tag_my_idme {
    u8 idme[ATAG_IDME_SIZE-3];
};

extern unsigned char system_idme[];

extern struct machine_desc * __init setup_machine_tags(phys_addr_t __atags_pointer,
                unsigned int machine_nr);

extern u32 g_devinfo_data[];
extern u32 g_devinfo_data_size;

//extern int __init parse_tag_core(const struct tag *tag);


static struct {
	struct tag_header core_header;
	struct tag_core core;
	struct tag_header mem_header;
	struct tag_mem32 mem;
	struct tag_header devinfo_data_header;
  struct tag_devinfo_data devinfo_data;
  struct tag_header idme_header;
  struct tag_my_idme idme;
  struct tag_header initrd_header;
  struct tag_initrd initrd;
  struct tag_header videolfb_header;
  struct tag_videolfb video_lfb;
  struct tag_header mdinfo_data_header;
  struct tag_mdinfo_data mdinfo_data;
  struct tag_header none_header;
} default_tags __initdata = {
	{ 2, ATAG_CORE },
	{ 3, ATAG_BOOT, 0x00 },
	{ tag_size(tag_mem32), ATAG_MEM },
  { MY_MEM_SIZE, PHYS_OFFSET },
  { tag_size(tag_devinfo_data), ATAG_DEVINFO_DATA },
  { {0}, ATAG_DEVINFO_DATA_SIZE },
  { tag_size(tag_idme), ATAG_IDME },
  { { 0 } },
  { tag_size(tag_initrd), ATAG_INITRD2 },
  { 0x84000000, 0x0032bbd2 },
  { tag_size(tag_videolfb), ATAG_VIDEOLFB },
  { 0x00, 0x00, 0x00, 0x00, 0xb7a00000, 0x01000000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { tag_size(tag_mdinfo_data), ATAG_MDINFO_DATA },
  { { 0 } },
	{ 0, ATAG_NONE }
};

struct buffer {
	size_t size;
	char data[];
};

static ssize_t atags_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct buffer *b = PDE_DATA(file_inode(file));
	return simple_read_from_buffer(buf, count, ppos, b->data, b->size);
}

static const struct file_operations atags_fops = {
	.read = atags_read,
	.llseek = default_llseek,
};

//#define BOOT_PARAMS_SIZE 1536
#define BOOT_PARAMS_SIZE  3 * PAGE_SIZE
//char atags_copy[BOOT_PARAMS_SIZE];
char *atags_copy;

void __init save_atags(struct tag *tags)
{
	memcpy(default_tags.devinfo_data.devinfo_data, g_devinfo_data, ATAG_DEVINFO_DATA_SIZE * 4); //g_devinfo_data_size);
	memcpy(default_tags.idme.idme, system_idme, IDME_ATAG_SIZE);
}

static int __init init_atags_procfs(void)
{
	/*
	 * This cannot go into save_atags() because kmalloc and proc don't work
	 * yet when it is called.
	 */
	struct proc_dir_entry *tags_entry;
	struct tag *tag;// = (struct tag *)atags_copy;
	struct buffer *b;
	size_t size;

	struct tag *tags = (struct tag *)&default_tags;

  printk("[k4y0z]: reassemble atags for proc\n");
  save_atags(tags);
  tag = tags;
  
	if (tag->hdr.tag != ATAG_CORE) {
		printk(KERN_INFO "No ATAGs?");
		return -EINVAL;
	}
  
  
	for (; tag->hdr.size; tag = tag_next(tag))
		;
  
	/* include the terminating ATAG_NONE */
	size = (char *)tag - (char *)tags + sizeof(struct tag_header);

	WARN_ON(tag->hdr.tag != ATAG_NONE);

	b = kmalloc(sizeof(*b) + size, GFP_KERNEL);
	if (!b)
		goto nomem;

	b->size = size;
	memcpy(b->data, tags, size);

	tags_entry = proc_create_data("atags", 0400, NULL, &atags_fops, b);
	if (!tags_entry)
		goto nomem;

	return 0;

nomem:
	kfree(b);
	printk(KERN_ERR "Exporting ATAGs: not enough memory\n");

	return -ENOMEM;
}
module_init(init_atags_procfs);
