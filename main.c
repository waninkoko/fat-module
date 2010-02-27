/*   
	Custom IOS Module (FAT)

	Copyright (C) 2008 neimod.
	Copyright (C) 2009 WiiGator.
	Copyright (C) 2009 Waninkoko.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <string.h>
#include <errno.h>

#include "fat.h"
#include "fat_wrapper.h"
#include "ipc.h"
#include "mem.h"
#include "module.h"
#include "sdio.h"
#include "syscalls.h"
#include "timer.h"
#include "types.h"
#include "usbstorage.h"


s32 __FAT_Initialize(u32 *queuehandle)
{
	void *buffer = NULL;
	s32   ret;

	/* Initialize memory heap */
	Mem_Init();

	/* Initialize timer subsystem */
	Timer_Init();

	/* Allocate queue buffer */
	buffer = Mem_Alloc(0x80);
	if (!buffer)
		return IPC_ENOMEM;

	/* Create message queue */
	ret = os_message_queue_create(buffer, 32);
	if (ret < 0)
		return ret;

	/* Register devices */
	os_device_register(DEVICE_FAT,  ret);
	os_device_register(DEVICE_SDIO, ret);
	os_device_register(DEVICE_USB,  ret);

	/* Copy queue handler */
	*queuehandle = ret;

	return 0;
}


s32 FAT_Ioctl(s32 fd, u32 cmd, void *inbuf, u32 inlen, void *iobuf, u32 iolen)
{
	s32 ret = IPC_EINVAL;

	/* Invalidate cache */
	if (inbuf)
		os_sync_before_read(inbuf, inlen);

	/* Parse command */
	switch (cmd) {
	/** Get file stats **/
	case IOCTL_FAT_FILESTATS: {
		fstats *stats = (fstats *)iobuf;

		/* Get file stats */
		ret = FAT_GetFileStats(fd, stats);

		break;
	}

	default:
		break;
	}

	/* Flush cache */
	if (iobuf)
		os_sync_after_write(iobuf, iolen);

	return ret;
}

s32 FAT_Ioctlv(s32 fd, u32 cmd, ioctlv *vector, u32 inlen, u32 iolen)
{
	s32 ret = IPC_EINVAL;

	/* Invalidate ache */
	InvalidateVector(vector, inlen, iolen);

	/* Parse command */
	switch (cmd) {
	/** Create directory **/
	case IOCTL_FAT_MKDIR: {
		char *dirpath = (char *)vector[0].data;

		/* Create directory */
		ret = FAT_CreateDir(dirpath);

		break;
	}

	/** Create file **/
	case IOCTL_FAT_MKFILE: {
		char *filepath = (char *)vector[0].data;

		/* Create file */
		ret = FAT_CreateFile(filepath);

		break;
	}

	/** Read directory **/
	case IOCTL_FAT_READDIR: {
		char *dirpath = (char *)vector[0].data;
		char *outbuf  = NULL;
		u32  *outlen  = NULL;
		u32   buflen  = 0;

		u32 entries = 0;

		/* Input values */
		if (iolen > 1) {
			entries = *(u32 *)vector[1].data;
			outbuf  = (char *)vector[2].data;
			outlen  =  (u32 *)vector[3].data;
			buflen  =         vector[2].len;
		} else
			outlen  =  (u32 *)vector[1].data;

		/* Clear buffer */
		if (outbuf)
			memset(outbuf, 0, buflen);

		/* Read directory */
		ret = FAT_ReadDir(dirpath, outbuf, outlen, entries);

		break;
	}

	/** Read directory (LFN) **/
	case IOCTL_FAT_READDIR_LFN: {
		char *dirpath = (char *)vector[0].data;
		char *outbuf  = NULL;
		u32  *outlen  = NULL;
		u32   buflen  = 0;

		u32 entries = 0;

		/* Input values */
		if (iolen > 1) {
			entries = *(u32 *)vector[1].data;
			outbuf  = (char *)vector[2].data;
			outlen  =  (u32 *)vector[3].data;
			buflen  =         vector[2].len;
		} else
			outlen  =  (u32 *)vector[1].data;

		/* Clear buffer */
		if (outbuf)
			memset(outbuf, 0, buflen);

		/* Read directory (LFN) */
		ret = FAT_ReadDirLfn(dirpath, outbuf, outlen, entries);

		break;
	}

	/** Delete object **/
	case IOCTL_FAT_DELETE: {
		char *path = (char *)vector[0].data;

		/* Delete file/directory */
		ret = FAT_Delete(path);

		break;
	}

	/** Delete directory **/
	case IOCTL_FAT_DELETEDIR: {
		char *dirpath = (char *)vector[0].data;

		/* Delete directory */
		ret = FAT_DeleteDir(dirpath);

		break;
	}

	/** Rename object **/
	case IOCTL_FAT_RENAME: {
		char *oldname = (char *)vector[0].data;
		char *newname = (char *)vector[1].data;

		/* Rename file/directory */
		ret = FAT_Rename(oldname, newname);

		break;
	}

	/** Get stats **/
	case IOCTL_FAT_STAT: {
		char *path  = (char *)vector[0].data;
		void *stats = vector[1].data;

		/* Get stats */
		ret = FAT_Stat(path, stats);

		break;
	}

	/** Get filesystem stats **/
	case IOCTL_FAT_VFSSTATS: {
		char *path  = (char *)vector[0].data;
		void *stats = vector[1].data;

		/* Get filesystem stats */
		ret = FAT_GetVfsStats(path, stats);

		break;
	}

	/** Get usage **/
	case IOCTL_FAT_GETUSAGE: {
		char *path   = (char *)vector[0].data;
		u32  *blocks =  (u32 *)vector[1].data;
		u32  *inodes =  (u32 *)vector[2].data;

		u64 size;

		/* Reset values */
		*blocks = 0;
		*inodes = 0;

		/* Get usage */
		ret = FAT_GetUsage(path, &size, inodes);

		/* Not a directory */
		if (ret == -ENOTDIR) {
			ret = 0;
			break;
		}

		/* Calculate blocks */
		*blocks = (size / 0x4000);

		break;
	}


	/** Mount SD card **/
	case IOCTL_FAT_MOUNTSD: {
		/* Initialize SDIO */
		ret = !__io_wiisd.startup();
		if (ret)
			break;

		/* Mount SD card */
		ret = !fatMountSimple("sd", &__io_wiisd);

		break;
	}

	/** Unmount SD card **/
	case IOCTL_FAT_UMOUNTSD: {
		/* Unmount SD card */
		fatUnmount("sd");

		/* Deinitialize SDIO */
		ret = !__io_wiisd.shutdown();

		break;
	}

	/** Mount USB card **/
	case IOCTL_FAT_MOUNTUSB: {
		/* Initialize USB */
		ret = !__io_usbstorage.startup();
		if (ret)
			break;

		/* Mount USB */
		ret = !fatMountSimple("usb", &__io_usbstorage);

		break;
	}

	/** Unmount USB card **/
	case IOCTL_FAT_UMOUNTUSB: {
		/* Unmount USB */
		fatUnmount("usb");

		/* Deinitialize USB */
		ret = !__io_usbstorage.shutdown();

		break;
	}

	default:
		break;
	}

	/* Flush cache */
	FlushVector(vector, inlen, iolen);

	return ret;
}

int main(void)
{
	u32 queuehandle;
	s32 ret;

	/* Print info */
	write("$IOSVersion: FAT: " __DATE__ " " __TIME__ " 64M$\n");

	/* Initialize module */
	ret = __FAT_Initialize(&queuehandle);
	if (ret < 0)
		return ret;

	/* Main loop */
	while (1) {
		ipcmessage *message = NULL;

		/* Wait for message */
		os_message_queue_receive(queuehandle, (void *)&message, 0);

		switch (message->command) {
		case IOS_OPEN: {
			char *devpath = message->open.device;
			u32   mode    = message->open.mode;

			/* FAT device */
			if (!strcmp(devpath, DEVICE_FAT)) {
				ret = 0;
				break;
			}

			/* Open file */
			ret = FAT_Open(devpath, mode);

			break;
		}

		case IOS_CLOSE: {
			/* Close file */
			ret = FAT_Close(message->fd);

			break;
		}

		case IOS_READ: {
			void *buffer = message->read.data;
			u32   len    = message->read.length;

			/* Read file */
			ret = FAT_Read(message->fd, buffer, len);

			break;
		}

		case IOS_WRITE: {
			void *buffer = message->write.data;
			u32   len    = message->write.length;

			/* Write file */
			ret = FAT_Write(message->fd, buffer, len);

			break;
		}

		case IOS_SEEK: {
			u32 where  = message->seek.offset;
			u32 whence = message->seek.origin;

			/* Seek file */
			ret = FAT_Seek(message->fd, where, whence);

			break;
		}

		case IOS_IOCTL: {
			void *inbuf = message->ioctl.buffer_in;
			u32   inlen = message->ioctl.length_in;
			void *iobuf = message->ioctl.buffer_io;
			u32   iolen = message->ioctl.length_io;
			u32   cmd   = message->ioctl.command;

			/* Parse IOCTL message */
			ret = FAT_Ioctl(message->fd, cmd, inbuf, inlen, iobuf, iolen);

			break;
		}

		case IOS_IOCTLV: {
			ioctlv *vector = message->ioctlv.vector;
			u32     inlen  = message->ioctlv.num_in;
			u32     iolen  = message->ioctlv.num_io;
			u32     cmd    = message->ioctlv.command;

			/* Parse IOCTLV message */
			ret = FAT_Ioctlv(message->fd, cmd, vector, inlen, iolen);

			break;
		}

		default:
			/* Unknown command */
			ret = IPC_EINVAL;
		}

		/* Acknowledge message */
		os_message_queue_ack((void *)message, ret);
	}
   
	return 0;
}
