#include <libudev.h>
#include <stdio.h>

static struct udev *udev = NULL;
int foo() 
{
	struct udev_device *dev = NULL;
	
	if (!udev)
		udev = udev_new();
	dev = udev_device_new_from_subsystem_sysname(udev, "block", "/dev/sda");
}
