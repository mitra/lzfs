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
#include <sys/lzfs_exportfs.h>


static int lzfs_encode_fh(struct dentry *dentry, u32 *fh, int *max_len, int connectable)
{
        struct lzfs_fid  *lzfid = (struct lzfs_fid *)fh;
        struct inode *inode = dentry->d_inode;
        int lfid_type;

        /* Directories don't need their parent encoded, they have ".." */
        if (S_ISDIR(inode->i_mode) || !connectable)
        {
                lzfid->ino = inode->i_ino;
                lzfid->gen = inode->i_generation;
                lfid_type = LZFS_FILEID_INO64_GEN;
        }
        else
        {
                spin_lock(&dentry->d_lock);
                lzfid->parent_ino = dentry->d_parent->d_inode->i_ino;
                lzfid->parent_gen = dentry->d_parent->d_inode->i_generation;
                spin_unlock(&dentry->d_lock);
                lfid_type = LZFS_FILEID_INO64_GEN_PARENT;
        }
        return lfid_type;
}


struct dentry * lzfs_fh_to_dentry(struct super_block *sb, struct fid *fid,
                                 int fh_len, int fh_type)
{
        struct lzfs_fid  *lzfid = (struct lzfs_fid *)fid;
        struct inode *inode = NULL;
	u32 i_generation = 0;

        if (fh_len < 2)
        {
                return NULL;
        }

        switch (fh_type)
        {
                case LZFS_FILEID_INO64_GEN :
                        inode = iget_locked(sb, lzfid->ino);
                        if (!inode)
                                return NULL;
			i_generation = lzfid->gen;
                        break;
                case LZFS_FILEID_INO64_GEN_PARENT :
                        inode = iget_locked(sb, lzfid->parent_ino);
                        if (!inode)
                                return NULL;
			i_generation = lzfid->parent_gen;
                        break;
        }

        if (inode->i_state & I_NEW)
                unlock_new_inode(inode);

	if (inode->i_generation != i_generation)
		return NULL;

        return d_obtain_alias(inode);
}

struct dentry *lzfs_get_parent(struct dentry *child)
{
        unsigned long p_ino;
        struct inode *inode = NULL;

        printk(KERN_WARNING "In zfs_get_parent\n");
        p_ino = parent_ino(child);
        inode = iget_locked(child->d_inode->i_sb, p_ino);

        if(!inode)
                return NULL;

        if (inode->i_state & I_NEW)
                unlock_new_inode(inode);

        return d_obtain_alias(inode);
}

const struct export_operations zfs_export_ops = {
        .encode_fh      = lzfs_encode_fh,
        .fh_to_dentry   = lzfs_fh_to_dentry,
        .fh_to_parent   = lzfs_fh_to_dentry,
        .get_parent     = lzfs_get_parent,
};
