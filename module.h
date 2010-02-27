#ifndef _MODULE_H_
#define _MODULE_H_

/* IOCTL commands */
#define IOCTL_FAT_FILESTATS	11

/* IOCTLV commands */
#define IOCTL_FAT_MKDIR		0x01
#define IOCTL_FAT_MKFILE	0x02
#define IOCTL_FAT_READDIR	0x03
#define IOCTL_FAT_READDIR_LFN	0x04
#define IOCTL_FAT_DELETE	0x05
#define IOCTL_FAT_DELETEDIR	0x06
#define IOCTL_FAT_RENAME	0x07
#define IOCTL_FAT_STAT		0x08
#define IOCTL_FAT_VFSSTATS	0x09
#define IOCTL_FAT_GETUSAGE	0x0A
#define IOCTL_FAT_MOUNTSD	0xF0
#define IOCTL_FAT_UMOUNTSD	0xF1
#define IOCTL_FAT_MOUNTUSB	0xF2
#define IOCTL_FAT_UMOUNTUSB	0xF3
#define IOCTL_FAT_SHUTDOWN	0xFF

/* Device names */
#define DEVICE_FAT		"fat"
#define DEVICE_SDIO		"sd:"
#define DEVICE_USB		"usb:"

#endif
