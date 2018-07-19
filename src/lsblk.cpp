#include <libudev.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <crow.h>
#include "path.h"
#include <string.h>


/* SCSI device types.  Copied almost as-is from kernel header.
 *  * http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/include/scsi/scsi.h */
#define SCSI_TYPE_DISK                  0x00
#define SCSI_TYPE_TAPE                  0x01
#define SCSI_TYPE_PRINTER               0x02
#define SCSI_TYPE_PROCESSOR             0x03    /* HP scanners use this */
#define SCSI_TYPE_WORM                  0x04    /* Treated as ROM by our system */
#define SCSI_TYPE_ROM                   0x05
#define SCSI_TYPE_SCANNER               0x06
#define SCSI_TYPE_MOD                   0x07    /* Magneto-optical disk - treated as SCSI_TYPE_DISK */
#define SCSI_TYPE_MEDIUM_CHANGER        0x08
#define SCSI_TYPE_COMM                  0x09    /* Communications device */
#define SCSI_TYPE_RAID                  0x0c
#define SCSI_TYPE_ENCLOSURE             0x0d    /* Enclosure Services Device */
#define SCSI_TYPE_RBC                   0x0e
#define SCSI_TYPE_OSD                   0x11
#define SCSI_TYPE_NO_LUN                0x7f

struct blkdev_cxt {
	struct path_cxt *sysfs;
	char *name;
	int partition;
};


const char *blkdev_scsi_type_to_name(int type);

void list_blk() {
	//read all the blocks;
	//set_cxt
	//get result to upper level;
}

static struct udev *udev = NULL;

static int is_dm(const char *name)
{
        return strncmp(name, "dm-", 3) ? 0 : 1;
}

int udev_info(struct blkdev_cxt * cxt) 
{
	struct udev_device *dev = NULL;
	
	if (!udev)
		udev = udev_new();
	dev = udev_device_new_from_subsystem_sysname(udev, "block", cxt->name);
	const char * data = udev_device_get_property_value(dev, "ID_FS_TYPE");
	std::string s;
	if (data)
		s = data;
	CROW_LOG_INFO << "ID_FS_TYPE:" << s;
	udev_device_unref(dev);

}



static char *get_type(struct blkdev_cxt * cxt)
{
    char *res = NULL, *p;

    if (is_dm(cxt->name)) {
        char *dm_uuid = NULL;

        /* The DM_UUID prefix should be set to subsystem owning
         * the device - LVM, CRYPT, DMRAID, MPATH, PART */
        if (ul_path_read_string(cxt->sysfs, &dm_uuid, "dm/uuid") > 0
            && dm_uuid) {
            char *tmp = dm_uuid;
            char *dm_uuid_prefix = strsep(&tmp, "-");

            if (dm_uuid_prefix) {
                /* kpartx hack to remove partition number */
                if (strncasecmp(dm_uuid_prefix, "part", 4) == 0)
                    dm_uuid_prefix[4] = '\0';

                res = strdup(dm_uuid_prefix);
            }
        }

        free(dm_uuid);
        if (!res)
            /* No UUID or no prefix - just mark it as DM device */
            res = strdup("dm");

    } else if (!strncmp(cxt->name, "loop", 4)) {
        res = strdup("loop");

    } else if (!strncmp(cxt->name, "md", 2)) {
        char *md_level = NULL;

        ul_path_read_string(cxt->sysfs, &md_level, "md/level");
        res = md_level ? md_level : strdup("md");

    } else {
        const char *type = NULL;
        int x = 0;

        if (ul_path_read_s32(cxt->sysfs, &x, "device/type") == 0)
            type = blkdev_scsi_type_to_name(x);
        if (!type)
            type = cxt->partition ? "part" : "disk";
        res = strdup(type);
    }

    for (p = res; p && *p; p++)
        *p = tolower((unsigned char) *p);
    return res;
}

const char *blkdev_scsi_type_to_name(int type)
{
        switch (type) {
        case SCSI_TYPE_DISK:
                return "disk";
        case SCSI_TYPE_TAPE:
                return "tape";
        case SCSI_TYPE_PRINTER:
                return "printer";
        case SCSI_TYPE_PROCESSOR:
                return "processor";
        case SCSI_TYPE_WORM:
                return "worm";
        case SCSI_TYPE_ROM:
                return "rom";
        case SCSI_TYPE_SCANNER:
                return "scanner";
        case SCSI_TYPE_MOD:
                return "mo-disk";
        case SCSI_TYPE_MEDIUM_CHANGER:
                return "changer";
        case SCSI_TYPE_COMM:
                return "comm";
        case SCSI_TYPE_RAID:
                return "raid";
        case SCSI_TYPE_ENCLOSURE:
                return "enclosure";
        case SCSI_TYPE_RBC:
                return "rbc";
        case SCSI_TYPE_OSD:
                return "osd";
        case SCSI_TYPE_NO_LUN:
                return "no-lun";
        default:
                break;
        }
        return NULL;
}
