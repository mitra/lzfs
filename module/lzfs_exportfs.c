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

#include <linux/fs.h>
#include <sys/vfs.h>
#include <sys/lzfs_exportfs.h>
#include <sys/tsd_wrapper.h>

extern int zfs_fid(vnode_t *vp, fid_t *fidp, caller_context_t *ct);
extern int zfs_vget(vfs_t *vfsp, vnode_t **vpp, fid_t *fidp);
extern int zfs_lookup(vnode_t *dvp, char *nm, vnode_t **vpp, struct pathname *pnp,
    int flags, vnode_t *rdir, cred_t *cr,  caller_context_t *ct,
    int *direntflags, pathname_t *realpnp);

static int lzfs_encode_fh(struct dentry *dentry, u32 *fh, int *max_len, int connectable)
{
        struct lzfs_fid  *lzfid = (struct lzfs_fid *)fh;
        struct inode *inode = dentry->d_inode;
        int lfid_type = LZFS_FILEID_INO64_GEN;
	vnode_t *vp;
	int error = 0;

	ENTRY;
	lzfid->fid_len = *max_len;
	if (!(S_ISDIR(inode->i_mode) || !connectable))
	{
		spin_lock(&dentry->d_lock);
		inode = dentry->d_parent->d_inode;
		spin_unlock(&dentry->d_lock);
		lfid_type = LZFS_FILEID_INO64_GEN_PARENT;
	}

	vp = LZFS_ITOV(inode);
	error = zfs_fid( vp, lzfid, 0);
        tsd_exit();
        EXIT;

	if (error)
	{
		printk(KERN_WARNING "Unable to get file handle \n");
		return -ENOSPC;
	}

	*max_len = lzfid->fid_len;

        return lfid_type;
}

struct dentry * lzfs_fh_to_dentry(struct super_block *sb, struct fid *fid,
                                 int fh_len, int fh_type)
{
        struct lzfs_fid  *lzfid = (struct lzfs_fid *)fid;
	vfs_t *vfsp = sb->s_fs_info;
	vnode_t *vp;
	int error = 0;
	struct dentry *dentry = NULL;

	ENTRY;
        if (fh_len < 2)
        {
                return NULL;
        }

        switch (fh_type)
        {
                case LZFS_FILEID_INO64_GEN :
                case LZFS_FILEID_INO64_GEN_PARENT :
			error = zfs_vget( vfsp, &vp, lzfid);
			break;
        }

	tsd_exit();
	EXIT;
	if (error)
	{
		printk(KERN_WARNING "Unable to get vnode \n");
		return NULL;
	}

	if (LZFS_VTOI(vp))
		dentry = d_obtain_alias(LZFS_VTOI(vp));
        return dentry;
}

struct dentry *lzfs_get_parent(struct dentry *child)
{
	vnode_t *vcp = LZFS_ITOV(child->d_inode);
	vnode_t *vp;
	int error = 0;
	struct dentry *dentry = NULL;
	const struct cred *cred = get_current_cred();

	ENTRY;
	error = zfs_lookup(vcp, "..", &vp, NULL, 0 , NULL,
                        (struct cred *) cred, NULL, NULL, NULL);

        put_cred(cred);
        tsd_exit();
        EXIT;
        if (error) {
                if (error == ENOENT)
		{
			printk(KERN_WARNING "Try to get new dentry \n");
                        return d_splice_alias(NULL, dentry);
		}
                else   
		{
			printk(KERN_WARNING "Unable to get dentry \n");
                        return ERR_PTR(-error);
		}
        }

        if (LZFS_VTOI(vp))
                dentry = d_obtain_alias(LZFS_VTOI(vp));
        return dentry;
}

const struct export_operations zfs_export_ops = {
        .encode_fh      = lzfs_encode_fh,
        .fh_to_dentry   = lzfs_fh_to_dentry,
        .fh_to_parent   = lzfs_fh_to_dentry,
        .get_parent     = lzfs_get_parent,
};
