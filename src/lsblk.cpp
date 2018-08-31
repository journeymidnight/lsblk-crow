#include <libudev.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <crow.h>
#include "path.h"
#include <string.h>
#include <libmount/libmount.h>


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


#define _PATH_SYS_DEVBLOCK "/sys/dev/block"
#define _PATH_SYS_BLOCK        "/sys/block"


void xfree(void * p){
	if (p != NULL) {
		free(p);
	}
}


struct blkdev_cxt {
	struct path_cxt *sysfs;
	char *name;
	char *filename;
	char *type;
	char *fstype;
	char *mountpoint;
	int maj;
	int min;
	uint64_t size;
	int nholders;
	int nslaves;
	int npartitions;
        int partition;
        int cephmount;
	
	//functions
	void reset();
	~blkdev_cxt();
};

blkdev_cxt::~blkdev_cxt() {
	this->reset();
}

void blkdev_cxt::reset() {
	xfree(name);
	xfree(type);
	xfree(fstype);
	xfree(mountpoint);

	xfree(filename);
	ul_unref_path(this->sysfs);
	//reset all field to zero
	memset(this, 0, sizeof(*this));
}

const char *blkdev_scsi_type_to_name(int type);
static char *get_type(struct blkdev_cxt * cxt);
static struct dirent *xreaddir(DIR *dp);
static int set_cxt(struct blkdev_cxt *cxt, struct blkdev_cxt *wholedisk, const char * name);
static char * udev_fstype_info(struct blkdev_cxt * cxt); 

static struct libmnt_table *mtab, *swaps;
static struct libmnt_cache *mntcache;

// copy from libmount
void mnt_unref_table(struct libmnt_table *tb)
{
    if (tb) {
        //tb->refcount--;
        /*DBG(FS, ul_debugobj(tb, "unref=%d", tb->refcount));*/
        mnt_free_table(tb);
    }
}

// copy from libmount
void mnt_unref_cache(struct libmnt_cache *cache)
{
    if (cache) {
        //cache->refcount--;
        /*DBG(CACHE, ul_debugobj(cache, "unref=%d", cache->refcount));*/
        //mnt_unref_table(cache->mtab);

        mnt_free_cache(cache);
    }
}


static int is_active_swap(const char *filename)
{
    if (!swaps) {
        swaps = mnt_new_table();
        if (!swaps)
            return 0;
        if (!mntcache)
            mntcache = mnt_new_cache();

        mnt_table_set_cache(swaps, mntcache);

        mnt_table_parse_swaps(swaps, NULL);
    }

    return mnt_table_find_srcpath(swaps, filename, MNT_ITER_BACKWARD) != NULL;
}

static char *get_device_mountpoint(struct blkdev_cxt *cxt)
{
    struct libmnt_fs *fs;
    const char *fsroot;

    if (!mtab) {
        mtab = mnt_new_table();
        if (!mtab)
            return NULL;
        if (!mntcache)
            mntcache = mnt_new_cache();

        mnt_table_set_cache(mtab, mntcache);
        mnt_table_parse_mtab(mtab, NULL);
    }

    /* Note that maj:min in /proc/self/mountinfo does not have to match with
     * devno as returned by stat(), so we have to try devname too
     */
    fs = mnt_table_find_devno(mtab, makedev(cxt->maj, cxt->min), MNT_ITER_BACKWARD);
    if (!fs)
        fs = mnt_table_find_srcpath(mtab, cxt->filename, MNT_ITER_BACKWARD);
    if (!fs)
        return is_active_swap(cxt->filename) ? strdup("[SWAP]") : NULL;

    /* found */
    fsroot = mnt_fs_get_root(fs);
    if (fsroot && strcmp(fsroot, "/") != 0) {
        /* hmm.. we found bind mount or btrfs subvolume, let's try to
         * get real FS root mountpoint */
        struct libmnt_fs *rfs;
        struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_BACKWARD);

        mnt_table_set_iter(mtab, itr, fs);
        while (mnt_table_next_fs(mtab, itr, &rfs) == 0) {
            fsroot = mnt_fs_get_root(rfs);
            if ((!fsroot || strcmp(fsroot, "/") == 0)
                && mnt_fs_match_source(rfs, cxt->filename, mntcache)) {
                fs = rfs;
                break;
            }
        }
        mnt_free_iter(itr);
    }

    return strdup(mnt_fs_get_target(fs));
}

int sysfs_blkdev_is_partition_dirent(DIR *dir, struct dirent *d, const char *parent_name)
{
    char path[255 + 6 + 1];

#ifdef _DIRENT_HAVE_D_TYPE
    if (d->d_type != DT_DIR &&
        d->d_type != DT_LNK &&
        d->d_type != DT_UNKNOWN)
        return 0;
#endif
    if (parent_name) {
        const char *p = parent_name;
        size_t len;

        /* /dev/sda --> "sda" */
        if (*parent_name == '/') {
            p = strrchr(parent_name, '/');
            if (!p)
                return 0;
            p++;
        }

        len = strlen(p);
        if (strlen(d->d_name) <= len)
            return 0;

        /* partitions subdir name is
         *    "<parent>[:digit:]" or "<parent>p[:digit:]"
         */
        return strncmp(p, d->d_name, len) == 0 &&
               ((*(d->d_name + len) == 'p' && isdigit(*(d->d_name + len + 1)))
            || isdigit(*(d->d_name + len)));
    }

    /* Cannot use /partition file, not supported on old sysfs */
    snprintf(path, sizeof(path), "%s/start", d->d_name);

    return faccessat(dirfd(dir), path, R_OK, 0) == 0;
}

static int sysfs_blkdev_count_partitions(struct path_cxt *pc, const char *devname)
{
    DIR *dir;
    struct dirent *d;
    int r = 0;

    dir = ul_path_opendir(pc, NULL);
    if (!dir)
        return 0;

    while ((d = xreaddir(dir))) {
        if (sysfs_blkdev_is_partition_dirent(dir, d, devname))
            r++;
    }

    closedir(dir);
    return r;
}

static struct dirent *xreaddir(DIR *dp)
{
    struct dirent *d;

    assert(dp);

    while ((d = readdir(dp))) {
        if (!strcmp(d->d_name, ".") ||
            !strcmp(d->d_name, ".."))
            continue;

        /* blacklist here? */
        break;
    }
    return d;
}

static int get_wholedisk_from_partition_dirent(DIR *dir,
                struct dirent *d, struct blkdev_cxt *cxt)
{
    char path[PATH_MAX];
    char *p;
    int len;

    if ((len = readlinkat(dirfd(dir), d->d_name, path, sizeof(path) - 1)) < 0)
        return 0;

    path[len] = '\0';

    /* The path ends with ".../<device>/<partition>" */
    p = strrchr(path, '/');
    if (!p)
        return 0;
    *p = '\0';

    p = strrchr(path, '/');
    if (!p)
        return 0;
    p++;

    return set_cxt(cxt, NULL, p);
}

static const char* sys_mount_dir[] = { 
        "/boot",
        "/",
        "/home",
        "/var",
        "/opt",
        "/mnt"
};

static bool has_sys_mount(struct blkdev_cxt *cxt)
{
        for (int i = 0; i < sizeof(sys_mount_dir)/sizeof(sys_mount_dir[0]); i++) {
                if (!strcmp(cxt->mountpoint, sys_mount_dir[i])) {
                        return true;
                }
        }
        return false;
}
static bool has_ceph_mount(struct blkdev_cxt *cxt)
{
        if (strstr(cxt->mountpoint, "ceph"))
        {
                return true;
        }
        return false;
}

static bool find_partitions(struct blkdev_cxt *wholedisk_cxt, const char *part_name);
static bool find_deps(struct blkdev_cxt *cxt);

static bool find_blkdev(struct blkdev_cxt *cxt, int do_partitions, const char *part_name)
{
        if (do_partitions && cxt->npartitions) {
                if (find_partitions(cxt, part_name)) {        /* partitions + whole-disk */
                        return true;
                }
        } else  {
                if (has_sys_mount(cxt)) {
                        return true;
                }
                if (has_ceph_mount(cxt)) {
                        cxt->cephmount = 1;
                }
        }

        return find_deps(cxt);
}

static bool find_partitions(struct blkdev_cxt *wholedisk_cxt, const char *part_name)
{
        DIR *dir;
        struct dirent *d;
        struct blkdev_cxt part_cxt = {NULL};
        
        if (!wholedisk_cxt->npartitions)
                return false;

        dir = ul_path_opendir(wholedisk_cxt->sysfs, NULL);
        if (!dir) {
                return false;
        }

        while ((d = xreaddir(dir)))
        {
                if (part_name && strcmp(part_name, d->d_name))
                        continue;
                if (!(sysfs_blkdev_is_partition_dirent(dir, d, wholedisk_cxt->name)))
                        continue;

                set_cxt(&part_cxt, wholedisk_cxt, d->d_name);
                if (has_sys_mount(wholedisk_cxt)) {
                        part_cxt.reset();
                        closedir(dir);
                        return true;
                }
                if (has_ceph_mount(wholedisk_cxt)) {
                        wholedisk_cxt->cephmount = 1;
                }
                bool ret;
                ret = find_blkdev(&part_cxt, 0, NULL);
                if (part_cxt.cephmount) {
                        wholedisk_cxt->cephmount = 1;
                }
                if (ret) {
                        part_cxt.reset();
                        closedir(dir);
                        return true;
                }

                part_cxt.reset();
        }

        closedir(dir);
        return false;
}

static bool find_deps(struct blkdev_cxt *cxt)
{
        DIR *dir;
        struct dirent *d;
        struct blkdev_cxt dep = { NULL };

        if (!cxt->nholders)
                return false;
        
        dir = ul_path_opendir(cxt->sysfs, "holders");
        if (!dir)
                return false;

        while ((d = xreaddir(dir))) {
                /* Is the dependency a partition? */
                if (sysfs_blkdev_is_partition_dirent(dir, d, NULL)) {
                        if (!get_wholedisk_from_partition_dirent(dir, d, &dep)) {
                                if (find_blkdev(&dep, 1, d->d_name)) {
                                        dep.reset();
                                        closedir(dir);
                                        return true;
                                }
                        }
                }
                /* The dependency is a whole device. */
                else if (!set_cxt(&dep, NULL, d->d_name)) {
                        if (find_blkdev(&dep, 1, NULL)) {
                                dep.reset();
                                closedir(dir);
                                return true;
                        }
                }
                dep.reset();
        }
        closedir(dir);

    return false;
}


void list_blk(crow::json::wvalue &json_result) {
	//read all the blocks;
	DIR *dir;
	struct dirent *d;
        struct blkdev_cxt cxt = { NULL };
	struct path_cxt *pc = ul_new_path(_PATH_SYS_BLOCK);
    mtab = NULL;
    swaps = NULL;
    mntcache = NULL;
	//set_cxt
	//get result to upper level;
	dir = ul_path_opendir(pc, NULL);
	int i = 0;
        while ((d = xreaddir(dir))) {
		set_cxt(&cxt, NULL, d->d_name);
                if (find_blkdev(&cxt, 1, NULL)) {
                        continue;
                }
		json_result[i]["name"] = cxt.name;
		json_result[i]["type"] = cxt.type;
		json_result[i]["npartitions"] = cxt.npartitions;
		json_result[i]["fstype"] = cxt.fstype;
		json_result[i]["mountpoint"] = cxt.mountpoint;
		json_result[i]["size"] = cxt.size;
                json_result[i]["cephmount"] = cxt.cephmount;
		cxt.reset();
		i ++;
	}

    closedir(dir);
	ul_unref_path(pc);
    mnt_unref_table(mtab);
    mnt_unref_table(swaps);
    mnt_unref_cache(mntcache);
}


static int is_dm(const char *name)
{
        return strncmp(name, "dm-", 3) ? 0 : 1;
}

static dev_t devname_to_devno(const char * name){
	char * buf = NULL;
	int dev = -1;
	struct stat st;
	asprintf(&buf, "/dev/%s", name);
	stat(buf, &st);
	dev = st.st_rdev;
	free(buf);
	return dev;
}


static struct path_cxt * new_sysfs_path(dev_t devno) {
    struct path_cxt *pc = ul_new_path(NULL);
    if (!pc)
	return NULL;
    char buf[sizeof(_PATH_SYS_DEVBLOCK)
         + sizeof(stringify_value(UINT32_MAX)) * 2
         + 3];

    snprintf(buf, sizeof(buf), _PATH_SYS_DEVBLOCK "/%d:%d", major(devno), minor(devno));

    int rc = ul_path_set_dir(pc, buf);
    if (rc)
        return NULL; 
    rc = ul_path_get_dirfd(pc);
    if (rc < 0)
        return NULL;
    return pc;
    

}

static int set_cxt(struct blkdev_cxt *cxt, struct blkdev_cxt *wholedisk, const char * name) {
	cxt->name = strdup(name);
	dev_t devno = devname_to_devno(name);
	cxt->maj = major(devno);
	cxt->min = minor(devno);
	cxt->sysfs = new_sysfs_path(devno);
	cxt->fstype = udev_fstype_info(cxt);
        cxt->partition = wholedisk != NULL;
	char * filename;
	asprintf(&filename, "/dev/%s", name);



	cxt->type = get_type(cxt);
	cxt->filename = filename;
        cxt->nholders = ul_path_count_dirents(cxt->sysfs, "holders");
        cxt->nslaves = ul_path_count_dirents(cxt->sysfs, "slaves");
        cxt->npartitions = sysfs_blkdev_count_partitions(cxt->sysfs, cxt->name);
	cxt->mountpoint = get_device_mountpoint(cxt);
	if(cxt->mountpoint == NULL){
		cxt->mountpoint = strdup("none");
	}
        if (ul_path_read_u64(cxt->sysfs, &cxt->size, "size") == 0)/* in sectors */
        	cxt->size <<= 9;
        cxt->cephmount = 0;
        return 0;
}



static struct udev *udev = NULL;
static char * udev_fstype_info(struct blkdev_cxt * cxt) 
{
	struct udev_device *dev = NULL;
	char * d;
	
	if (!udev)
		udev = udev_new();
	dev = udev_device_new_from_subsystem_sysname(udev, "block", cxt->name);
	const char * data = udev_device_get_property_value(dev, "ID_FS_TYPE");
	if(data)
		d = strdup(data);
	else 	
		d = strdup("none");
	udev_device_unref(dev);
	return d;
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
            type = "disk";
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
