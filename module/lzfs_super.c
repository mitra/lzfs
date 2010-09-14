/*
 *  This file is part of the LZPL: Linux ZFS Posix Layer
 *
 *  Copyright (c) 2010 Knowledge Quest Infotech Pvt. Ltd. 
 *  Produced at Knowledge Quest Infotech Pvt. Ltd. 
 *  Written by: Knowledge Quest Infotech Pvt. Ltd. 
 *              zfs@kqinfotech.com
 *
 *  This is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/statfs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/lzfs_inode.h>
#include <sys/lzfs_snap.h>
#include <sys/lzfs_exportfs.h>

#include <sys/mntent.h>

#ifdef DEBUG_SUBSYSTEM
#undef DEBUG_SUBSYSTEM
#endif

#define DEBUG_SUBSYSTEM S_LZFS

/* symbol exported by zfs module */
extern int zfs_domount(vfs_t *vfsp, char *data);
extern int zfs_root(vfs_t *vfsp, vnode_t **vvp); 
extern int zfs_umount(vfs_t *vfsp, int fflags, cred_t *cr); 
extern int zfs_statvfs(vfs_t *vfsp, struct statvfs64 *statp);
extern void lzfs_zfsctl_create(vfs_t *);
extern void lzfs_zfsctl_destroy(vfs_t *);

static void lzfs_delete_vnode(struct inode *inode)
{
	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
	inode->i_size = 0; 
}

static void 
lzfs_clear_vnode(struct inode *inode)
{
	vnode_t		*vp;

	ENTRY;
	vp = LZFS_ITOV(inode);
	
	ASSERT(vp->v_count == 1);
	
	/* znode associated with this vnode is freed by zfs_inactive.
	 *
	 * each snapshot psuedo-inode does not contain corresponding
	 * znode on-disk hence we need not call zfs_inactive for those
	 * inodes i_private field of those inode is set to struct inode
	 * and is not NULL to distinguish them with other inodes having
	 * znode
 	 */
	if(inode->i_ino != LZFS_ZFSCTL_INO_ROOT 
		&& inode->i_ino != LZFS_ZFSCTL_INO_SNAPDIR
		&& inode->i_private == NULL ) { 
			if(!((vfs_t *)inode->i_sb->s_fs_info)->is_snap) {
				zfs_inactive(vp, NULL, NULL);
			}
	}
	vp->v_data = NULL;
	EXIT;
}

static void
lzfs_put_super(struct super_block *sb)
{
	ENTRY;
	zfs_umount(sb->s_fs_info, 0, NULL);
	kfree(sb->s_fs_info);
	EXIT;
}

static struct inode *
lzfs_alloc_vnode(struct super_block *sb) 
{
	vnode_t *vp = NULL;
	
	ENTRY;
	vp = vn_alloc(KM_SLEEP); 
	bzero(vp, sizeof(vnode_t));
	mutex_init(&vp->v_lock, NULL, MUTEX_DEFAULT, NULL);
	inode_init_once(LZFS_VTOI(vp));
	LZFS_VTOI(vp)->i_version = 1;
	EXIT;
	return LZFS_VTOI(vp);
}

static void
lzfs_destroy_vnode(struct inode *inode)
{
	mutex_destroy(&(LZFS_ITOV(inode))->v_lock);
	vn_free(LZFS_ITOV(inode));	
}

/* Structure to keep all the zfs related callback routines.
 */

static inline vfs_t* lzfs_super(struct super_block *sb)
{
	return sb->s_fs_info;
}

static int lzfs_statfs(struct dentry *dentry, struct kstatfs *statfs)
{
	struct super_block *sb = dentry->d_sb;
	vfs_t *vfsp = lzfs_super(sb);
	struct statvfs64 stat;

	BUG_ON(zfs_statvfs(vfsp, &stat));

	statfs->f_type = vfsp->vfs_magic;
    statfs->f_bsize = stat.f_frsize;
	statfs->f_blocks = stat.f_blocks;
	statfs->f_bfree = stat.f_bfree;
	statfs->f_bavail = stat.f_bavail;
	statfs->f_files = stat.f_files;
	statfs->f_ffree = stat.f_ffree;
	statfs->f_namelen = stat.f_namemax;
	return 0;
}


static int lzfs_show_options(struct seq_file *seq, struct vfsmount *vfsmnt)
{
	vfs_t *vfsp = lzfs_super(vfsmnt->mnt_sb);
/*
	if (vfs_isreadonly(vfsp))
		seq_printf(seq, ",%s", MNTOPT_RO);
	else
		seq_printf(seq, ",%s", MNTOPT_RW);
*/

	if (vfs_isatime(vfsp))
		seq_printf(seq, ",%s", MNTOPT_ATIME);
	else {
		/* Linux Kernel Displays noatime when set, we need not do it */
		// seq_printf(seq, ",%s", MNTOPT_NOATIME);
	}

	if (vfs_isxattr(vfsp))
		seq_printf(seq, ",%s", MNTOPT_XATTR);
	else
		seq_printf(seq, ",%s", MNTOPT_NOXATTR);

	if (!vfs_issuid(vfsp)) {
		seq_printf(seq, ",%s", MNTOPT_NOSUID);
		// seq_printf(seq, ",%s", MNTOPT_NODEVICES);
		// seq_printf(seq, ",%s", MNTOPT_NOSETUID);
	} else {
		seq_printf(seq, ",%s", MNTOPT_SUID);
		if (vfs_isdevice(vfsp))
			seq_printf(seq, ",%s", MNTOPT_DEVICES);
		else {
			/* Linux Kernel Displays nodev by default */
			// seq_printf(seq, ",%s", MNTOPT_NODEVICES);
		}

		if (vfs_issetuid(vfsp))
			seq_printf(seq, ",%s", MNTOPT_SETUID);
		else
			seq_printf(seq, ",%s", MNTOPT_NOSETUID);
	}

	if (vfs_isexec(vfsp))
		seq_printf(seq, ",%s", MNTOPT_EXEC);
	else {
		/* Linux Kernel Displays noexec by default */
		// seq_printf(seq, ",%s", MNTOPT_NOEXEC);
	}
	return 0;
}

static const struct super_operations lzfs_ops = {
	.alloc_inode	=	lzfs_alloc_vnode,
	.clear_inode    =	lzfs_clear_vnode,
	.delete_inode   =   lzfs_delete_vnode,
	.destroy_inode	=	lzfs_destroy_vnode,
	.put_super	=	lzfs_put_super,
	.statfs		= 	lzfs_statfs,
	.show_options = lzfs_show_options,
};

static int 
lzfs_fill_super(struct super_block *sb, void *data, int silent)
{
	int error = 0;
	vfs_t *vfsp = NULL;
	vnode_t *root_vnode = NULL;
	struct inode *root_inode = NULL;
	struct dentry *root_dentry = NULL;
	long ret = -EINVAL;
	
	ENTRY;

	vfsp = (vfs_t *) kzalloc(sizeof(vfs_t), KM_SLEEP);
	vfsp->vfs_set_inode_ops = lzfs_set_inode_ops;
	vfsp->vfs_super   =	sb;
	sb->s_maxbytes	  =	MAX_LFS_FILESIZE;
	sb->s_op	  =	&lzfs_ops;
	sb->s_time_gran	  =	1;
	sb->s_flags	  =	MS_ACTIVE;
	sb->s_export_op	  =     &zfs_export_ops;
	error = zfs_domount(vfsp, data);
	if (error) {
		printk(KERN_WARNING "mount failed to open the pool!!\n");
		goto mount_failed;
	}
	
	vfsp->vfs_magic	  =	(uint32_t) ZFS_MAGIC;
	sb->s_fs_info	  =	vfsp;
	sb->s_magic	  =	vfsp->vfs_magic;
	if (!strchr((char *) data, '@')) {
		vfsp->is_snap = 0;
	} else {
		vfsp->is_snap = 1;
	}

	sb->s_blocksize   =	vfsp->vfs_bsize;
	sb->s_blocksize_bits = ilog2(vfsp->vfs_bsize);
	sb->s_time_gran = 1;


	zfs_root(sb->s_fs_info, &root_vnode); 
	if (!root_vnode) {
		printk(KERN_WARNING "root inode failed to allocate");
		goto mount_failed;
	}
	root_inode = &root_vnode->v_inode; 
	root_dentry = d_alloc_root(root_inode);
	if (!root_dentry) {
		printk(KERN_WARNING "chk4: %s\n", __FUNCTION__);
		goto mount_failed;
	}

	sb->s_root = root_dentry;
	if (!strchr((char *) data, '@')) {
		lzfs_zfsctl_create(vfsp);
	}
	EXIT;
	return 0;

mount_failed:
	sb->s_fs_info = NULL;
	kfree(vfsp);
	EXIT;
	return (ret);
}

extern int zfs_register_callbacks(vfs_t *vfsp);

static int 
lzfs_get_sb(struct file_system_type *fs_type,
	    int flags, const char *dev_name,
	    void *data, struct vfsmount *mnt)
{
	int rc;
	vfs_t *vfsp = NULL;

	/* get the pool/file-system name in the dev_name
	 * There is no need for a block device for this file system.
	 * Let's call get_sb_nodev.
	 */
	ENTRY;
	rc = get_sb_nodev(fs_type, flags, (void *)dev_name, 
			    lzfs_fill_super, mnt);

	if (rc)
		return rc;

	vfsp = lzfs_super(mnt->mnt_sb);
	vfsp->vfsmnt = mnt;

	/* copy the mount flags information (from Linux Kernel) to 
	 * zfs file system 
	 * */

	if (flags & MS_RDONLY)
		vfsp->vfs_flag |= VFS_RDONLY;
	else
		vfsp->vfs_flag &= ~VFS_RDONLY;

	if (flags & MS_NOSUID)
		vfsp->vfs_flag &= ~VFS_SUID;
	else
		vfsp->vfs_flag |= VFS_SUID;

	if (flags & MS_NODEV)
		vfsp->vfs_flag |= VFS_NODEVICES;
	else
		vfsp->vfs_flag &= ~VFS_NODEVICES;

	if (flags & MS_NOEXEC)
		vfsp->vfs_flag |= VFS_NOEXEC;
	else
		vfsp->vfs_flag &= ~VFS_NOEXEC;

	if (flags & MS_NOATIME)
		vfsp->vfs_flag &= ~VFS_ATIME;
	else
		vfsp->vfs_flag |= VFS_ATIME;

	if ((rc = zfs_register_callbacks(vfsp)))
		lzfs_zfsctl_destroy(vfsp->vfs_super->s_fs_info);

	EXIT;
	return rc;
}

static void 
lzfs_kill_sb(struct super_block *sb)
{
	vfs_t *vfsp;

	ENTRY;
        if(sb->s_fs_info) {
            vfsp = (vfs_t *) sb->s_fs_info;
            if (!vfsp->is_snap) {
                lzfs_zfsctl_destroy(sb->s_fs_info);
            }
        }
	kill_anon_super(sb);
	EXIT;
}

struct file_system_type lzfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "zfs",
	.get_sb		= lzfs_get_sb,
	.kill_sb	= lzfs_kill_sb,
};

static int 
init_lzfs_fs(void)
{
	return register_filesystem(&lzfs_fs_type);
}

static void __exit 
exit_lzfs_fs(void)
{
	unregister_filesystem(&lzfs_fs_type);
}

module_init(init_lzfs_fs)
module_exit(exit_lzfs_fs)

MODULE_LICENSE("GPL");
//MODULE_LICENSE("Proprietary");

