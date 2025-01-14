obj-m:=hello.o
#KERNEL_SOURCE=/lib/modules/$(shell uname -r)/build
KERNAL_SOURCE=/home/ensea/linux-socfgpa
CFLAGS_MODULE=-fno-pic

all :
	make -C $(KERNEL_SOURCE) M=$(PWD) modules
clean :
	make -C $(KERNEL_SOURCE) M=$(PWD) clean
install :
	make −C $(KERNEL_SOURCE) M=$(PWD) modules install

