#include <linux/module.h>		// module_init  module_exit
#include <linux/init.h>			// __init   __exit
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-bank.h>		// arch/arm/mach-s5pv210/include/mach/gpio-bank.h
#include <linux/string.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/cdev.h>



#define MYMAJOR		200
#define MYCNT		1
#define MYNAME		"testchar"

#define GPJ0CON		S5PV210_GPJ0CON
#define GPJ0DAT		S5PV210_GPJ0DAT

#define rGPJ0CON	*((volatile unsigned int *)GPJ0CON)
#define rGPJ0DAT	*((volatile unsigned int *)GPJ0DAT)

#define GPJ0CON_PA	0xe0200240
#define GPJ0DAT_PA 	0xe0200244

unsigned int *pGPJ0CON;
unsigned int *pGPJ0DAT;


int mymajor;
static dev_t mydev;
static struct cdev test_cdev;

char kbuf[100];			// 内核空间的buf


static int test_chrdev_open(struct inode *inode, struct file *file)
{
	// 这个函数中真正应该放置的是打开这个设备的硬件操作代码部分
	// 但是现在暂时我们写不了这么多，所以用一个printk打印个信息来做代表。
	printk(KERN_INFO "test_chrdev_open\n");
	
	rGPJ0CON = 0x11111111;
	rGPJ0DAT = ((0<<3) | (0<<4) | (0<<5));		// 亮
	
	return 0;
}

static int test_chrdev_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "test_chrdev_release\n");
	
	rGPJ0DAT = ((1<<3) | (1<<4) | (1<<5));
	
	return 0;
}

ssize_t test_chrdev_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	int ret = -1;
	
	printk(KERN_INFO "test_chrdev_read\n");
	
	ret = copy_to_user(ubuf, kbuf, count);
	if (ret)
	{
		printk(KERN_ERR "copy_to_user fail\n");
		return -EINVAL;
	}
	printk(KERN_INFO "copy_to_user success..\n");
	
	
	return 0;
}

// 写函数的本质就是将应用层传递过来的数据先复制到内核中，然后将之以正确的方式写入硬件完成操作。
static ssize_t test_chrdev_write(struct file *file, const char __user *ubuf,
	size_t count, loff_t *ppos)
{
	int ret = -1;
	
	printk(KERN_INFO "test_chrdev_write\n");

	// 使用该函数将应用层传过来的ubuf中的内容拷贝到驱动空间中的一个buf中
	//memcpy(kbuf, ubuf);		// 不行，因为2个不在一个地址空间中
	memset(kbuf, 0, sizeof(kbuf));
	ret = copy_from_user(kbuf, ubuf, count);
	if (ret)
	{
		printk(KERN_ERR "copy_from_user fail\n");
		return -EINVAL;
	}
	printk(KERN_INFO "copy_from_user success..\n");
	
	if (kbuf[0] == '1')
	{
		rGPJ0DAT = ((0<<3) | (0<<4) | (0<<5));
	}
	else if (kbuf[0] == '0')
	{
		rGPJ0DAT = ((1<<3) | (1<<4) | (1<<5));
	}
	
	
/*
	// 真正的驱动中，数据从应用层复制到驱动中后，我们就要根据这个数据
	// 去写硬件完成硬件的操作。所以这下面就应该是操作硬件的代码
	if (!strcmp(kbuf, "on"))
	{
		rGPJ0DAT = ((0<<3) | (0<<4) | (0<<5));
	}
	else if (!strcmp(kbuf, "off"))
	{
		rGPJ0DAT = ((1<<3) | (1<<4) | (1<<5));
	}
*/

	
	
	return 0;
}


// 自定义一个file_operations结构体变量，并且去填充
static const struct file_operations test_fops = {
	.owner		= THIS_MODULE,				// 惯例，直接写即可
	
	.open		= test_chrdev_open,			// 将来应用open打开这个设备时实际调用的
	.release	= test_chrdev_release,		// 就是这个.open对应的函数
	.write 		= test_chrdev_write,
	.read		= test_chrdev_read,
};


// 模块安装函数
static int __init chrdev_init(void)
{	
	int retval;
	
	printk(KERN_INFO "chrdev_init helloworld init\n");

/*
	// 在module_init宏调用的函数中去注册字符设备驱动
	// major传0进去表示要让内核帮我们自动分配一个合适的空白的没被使用的主设备号
	// 内核如果成功分配就会返回分配的主设备好；如果分配失败会返回负数
	mymajor = register_chrdev(0, MYNAME, &test_fops);
	if (mymajor < 0)
	{
		printk(KERN_ERR "register_chrdev fail\n");
		return -EINVAL;
	}
	printk(KERN_INFO "register_chrdev success... mymajor = %d.\n", mymajor);
*/	

	// 使用新的cdev接口来注册字符设备驱动
	// 新的接口注册字符设备驱动需要2步
	
	// 第1步：注册/分配主次设备号
	mydev = MKDEV(MYMAJOR, 0);
	retval = register_chrdev_region(mydev, MYCNT, MYNAME);
	if (retval) {
		printk(KERN_ERR "Unable to register minors for %s\n", MYNAME);
		return -EINVAL;
	}
	printk(KERN_INFO "register_chrdev_region success\n");
	// 第2步：注册字符设备驱动
	cdev_init(&test_cdev, &test_fops);
	retval = cdev_add(&test_cdev, mydev, MYCNT);
	if (retval) {
		printk(KERN_ERR "Unable to cdev_add\n");
		return -EINVAL;
	}
	printk(KERN_INFO "cdev_add success\n");

	
	// 使用动态映射的方式来操作寄存器
	if (!request_mem_region(GPJ0CON_PA, 4, "GPJ0CON"))
		return -EINVAL;
	if (!request_mem_region(GPJ0DAT_PA, 4, "GPJ0CON"))
		return -EINVAL;
	
	pGPJ0CON = ioremap(GPJ0CON_PA, 4);
	pGPJ0DAT = ioremap(GPJ0DAT_PA, 4);
	
	*pGPJ0CON = 0x11111111;
	*pGPJ0DAT = ((0<<3) | (0<<4) | (0<<5));		// 亮
	

	return 0;
}

// 模块下载函数
static void __exit chrdev_exit(void)
{
	printk(KERN_INFO "chrdev_exit helloworld exit\n");

	*pGPJ0DAT = ((1<<3) | (1<<4) | (1<<5));	
	
	// 解除映射
	iounmap(pGPJ0CON);
	iounmap(pGPJ0DAT);
	release_mem_region(GPJ0CON_PA, 4);
	release_mem_region(GPJ0DAT_PA, 4);

/*	
	// 在module_exit宏调用的函数中去注销字符设备驱动
	unregister_chrdev(mymajor, MYNAME);
*/	

	// 使用新的接口来注销字符设备驱动
	// 注销分2步：
	// 第一步真正注销字符设备驱动用cdev_del
	cdev_del(&test_cdev);
	// 第二步去注销申请的主次设备号
	unregister_chrdev_region(mydev, MYCNT);
}


module_init(chrdev_init);
module_exit(chrdev_exit);

// MODULE_xxx这种宏作用是用来添加模块描述信息
MODULE_LICENSE("GPL");				// 描述模块的许可证
MODULE_AUTHOR("aston");				// 描述模块的作者
MODULE_DESCRIPTION("module test");	// 描述模块的介绍信息
MODULE_ALIAS("alias xxx");			// 描述模块的别名信息








