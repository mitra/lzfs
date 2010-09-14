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
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/lzfs_snap.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/lzfs_inode.h>
extern void zfs_fs_name_fn(void *, char *);
extern int zfs_snapshot_list_next(void *, char *, uint64_t *,
				uint64_t *, boolean_t *);
extern void set_zfsvfs_ctldir(void *, vnode_t *);
extern void zfsctl_dir_destroy(void *);
extern uint64_t zfs_snapname_to_id(void *void_ptr, const char *snapname);

/*
 * Readdir implementation for the .zfs directory
 * which contains only two directories inside named
 * snapshot and shares. We are currently only done with
 * snapshot.
 */

static int
zfsctl_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct dentry *dentry = filp->f_path.dentry;
	u64 ino;
	int i = filp->f_pos;
	
	switch (i) {
	case 0:
		ino = dentry->d_inode->i_ino;
		if (filldir(dirent, ".", 1, i, ino, DT_DIR) < 0)
			break;
		filp->f_pos++;
		i++;
	case 1:
		ino = parent_ino(dentry);
		if (filldir(dirent, "..", 2, i, ino, DT_DIR) < 0)
			break;
		filp->f_pos++;
		i++;
	case 2:
		if (filldir(dirent, ZFS_SNAPDIR_NAME, strlen(ZFS_SNAPDIR_NAME), i, 
			    LZFS_ZFSCTL_INO_SNAPDIR, DT_DIR) < 0) 
		  break;
		filp->f_pos++;
	}
	return 0;
}

/*
 * Lookup defined for .zfs directory currently for dir name
 * snapshot only later need to do for shares dir
 */

static struct dentry *
zfsctl_lookup(struct inode *dir,struct dentry *dentry, struct nameidata *nd)
{
	struct inode *inode = NULL;

	if (dentry->d_name.len >= MAXNAMELEN) {
		return ERR_PTR(-ENAMETOOLONG);
	}
	if (strcmp(dentry->d_name.name, ZFS_SNAPDIR_NAME) == 0) {
		inode = ilookup(dir->i_sb, LZFS_ZFSCTL_INO_SNAPDIR);
		if(!inode) {
			printk("snapshot dir inode not found");
			return NULL;
		}
		return d_splice_alias(inode, dentry);
	} else {
		return d_splice_alias(NULL, dentry);
	}
}

/*
 * readdir for snapshot dir which contains directory entries 
 * for all snapshots created gets it from dmu_snapshot_list_next
 */

static int
snap_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	vnode_t *dir_vp;
	struct inode *dir = filp->f_path.dentry->d_inode;
	char snapname[MAXNAMELEN];
	uint64_t id, cookie;
	boolean_t case_conflict;
	int error, rc;
	vfs_t *vfsp = NULL;

	dir_vp = LZFS_ITOV(dir);
	vfsp = dir_vp->v_vfsp; 
	cookie = filp->f_pos;
	rc = error = 0;
	if (!filp->f_pos) {
		rc = filldir(dirent, ".", 1, filp->f_pos, dir->i_ino, DT_DIR);
		if(rc)
			goto done;
		filp->f_pos++;
	}
	if (filp->f_pos == 1) {
		rc = filldir(dirent, "..", 2, filp->f_pos,
					parent_ino(filp->f_path.dentry), DT_DIR);
		if(rc) {
			goto done;
		}
		filp->f_pos++;
	}

	while (!(error = zfs_snapshot_list_next(vfsp->vfs_data, snapname, &id,
					       &cookie, &case_conflict))) {
		ASSERT(id > 0);
		rc = filldir(dirent, snapname, strlen(snapname), filp->f_pos, 
					LZFS_ZFSCTL_INO_SHARES - id, DT_DIR);
		filp->f_pos = cookie; // next position ptr
	}
	if (error) {
		if (error == ENOENT) {
			return (0);
		}
		return PTR_ERR(ERR_PTR(-error));
	}

done:
	return 0;
}

static void*
snap_mountpoint_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct vfsmount *mnt = ERR_PTR(-ENOENT);
	vnode_t *dir_vp = NULL;
	char *snapname = NULL;
	char *zfs_fs_name = NULL;
	int rc = 0;
	vfs_t *vfsp = NULL;
	
	ASSERT(dentry->d_parent);
	dir_vp = LZFS_ITOV(dentry->d_parent->d_inode);
	vfsp = dir_vp->v_vfsp;
	ASSERT(vfsp);
	zfs_fs_name = kmem_alloc(MAXNAMELEN, KM_SLEEP);
	dput(nd->path.dentry);
	nd->path.dentry = dget(dentry);
	zfs_fs_name_fn(vfsp->vfs_data, zfs_fs_name);
	snapname = kmem_alloc(strlen(zfs_fs_name) +
			strlen(dentry->d_name.name) + 2, KM_SLEEP);
	snapname = strncpy(snapname, zfs_fs_name, strlen(zfs_fs_name) + 1);
	snapname = strcat(snapname, "@");
	snapname = strcat(snapname, dentry->d_name.name);
	mnt = vfs_kern_mount(&lzfs_fs_type, 0, snapname, NULL);
	mntget(mnt);
	rc = PTR_ERR(mnt);
	if (IS_ERR(mnt)) {
		goto out_err;
	}
	mnt->mnt_mountpoint = dentry;
	ASSERT(nd);
	rc = do_add_mount(mnt, &nd->path,
	nd->path.mnt->mnt_flags | MNT_READONLY, NULL);
	switch (rc) {
	case 0:
		path_put(&nd->path);
		nd->path.mnt = mnt;
		nd->path.dentry = dget(mnt->mnt_root);
		break;
	case -EBUSY: 
		/* someone else made a mount here whilst we were busy */
		while (d_mountpoint(nd->path.dentry) &&
			follow_down(&nd->path)) {
			;
		 }
		 rc = 0;
	default:
		mntput(mnt);
		break;
	}
	kfree(zfs_fs_name);
	kfree(snapname);
	return ERR_PTR(rc);
out_err:
	path_put(&nd->path);
	kfree(zfs_fs_name);
	kfree(snapname);
	return ERR_PTR(rc);
}

const struct inode_operations snap_mount_dir_inode_operations = {
	.follow_link	= snap_mountpoint_follow_link,
};

/*
 * Inode allocation for the snapshots directories which are
 * present insided the snapshot directory
 */

struct inode *
lzfs_snapshot_iget(struct super_block *sb, unsigned long ino)
{
	vnode_t *vp = NULL; 
	struct inode *inode;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;
	vp = LZFS_ITOV(inode);
	mutex_enter(&vp->v_lock);
	vp->v_count = 1;
	mutex_exit(&vp->v_lock);
	inode->i_mode |= (S_IFDIR | S_IRWXU);
	inode->i_uid = current->cred->uid;
	inode->i_gid = current->cred->gid;
	inode->i_version = 1;
	inode->i_op = &snap_mount_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	inode->i_sb = sb;
	inode->i_private = inode;
	unlock_new_inode(inode);
	return inode;
}		

/*
 * looks up for the snapshot directories inode and then mounts
 * the snapshot dataset on that pseudo - inode's dentry.
 */

static struct dentry *
snap_lookup(struct inode *dir,struct dentry *dentry, struct nameidata *nd)
{
	struct inode *inode = NULL;
	uint64_t id;
	vnode_t *dir_vp = NULL;
	vfs_t *vfsp = NULL;
	struct dentry *dentry_to_return = NULL;
	
	dir_vp = LZFS_ITOV(dir);
	vfsp = dir_vp->v_vfsp;
	if (dentry->d_name.len >= MAXNAMELEN) {
        return ERR_PTR(-ENAMETOOLONG);
	}
	if (!(id = zfs_snapname_to_id(vfsp->vfs_data, dentry->d_name.name))) {
		d_add(dentry, NULL);
		return NULL;
	}
	inode = lzfs_snapshot_iget(vfsp->vfs_super,
						LZFS_ZFSCTL_INO_SHARES - id);
	if (unlikely(IS_ERR(inode))) {
		return ERR_CAST(inode);
	}
	dentry_to_return = d_splice_alias(inode, dentry);	
	return dentry_to_return;
}

/*
 * .zfs dir fops
 */

const struct file_operations zfsctl_dir_file_operations = {
	.read       = generic_read_dir,
	.readdir    = zfsctl_readdir,
};

/*
 * .zfs dir iops
 */

const struct inode_operations zfsctl_dir_inode_operations = {
	.lookup     = zfsctl_lookup,
};

/*
 * snapshot dir fops
 */

const struct file_operations snap_dir_file_operations = {
	.read       = generic_read_dir,
	.readdir    = snap_readdir,
};

/*
 * snapshot dir iops
 */

struct inode_operations snap_dir_inode_operations = {
	.lookup     = snap_lookup,
};

/*
 * called from lzfs_fill_super and creates .zfs and snapshot
 * dirs psuedo-inodes and vnodes if the dataset to be mounted
 * is not a snapshot and sets their iops and fops.
 */

void
lzfs_zfsctl_create(vfs_t *vfsp)
{
	vnode_t *vp_zfsctl_dir = NULL, *vp_snap_dir = NULL;
	struct dentry *zfsctl_dir_dentry = NULL, *snap_dir_dentry = NULL;
	struct inode *inode_ctldir = NULL, *inode_snapdir = NULL;
	timestruc_t now;

	inode_ctldir = iget_locked(vfsp->vfs_super, LZFS_ZFSCTL_INO_ROOT);
	ASSERT(inode_ctldir != NULL);
	vp_zfsctl_dir = LZFS_ITOV(inode_ctldir);
	gethrestime(&now);
	ASSERT(inode_ctldir->i_state & I_NEW);
	mutex_enter(&vp_zfsctl_dir->v_lock);
	vp_zfsctl_dir->v_count = 1;
	VN_SET_VFS_TYPE_DEV(vp_zfsctl_dir, vfsp, VDIR, 0);
	bcopy(&now, &(vp_zfsctl_dir->v_inode.i_ctime), 
			sizeof (timestruc_t));
	bcopy(&now, &(vp_zfsctl_dir->v_inode.i_atime),
	      sizeof (timestruc_t));
	bcopy(&now,&(vp_zfsctl_dir->v_inode.i_mtime),sizeof (timestruc_t));
	inode_ctldir->i_uid = current->cred->uid;
	inode_ctldir->i_gid = current->cred->gid;
	inode_ctldir->i_version = 1;
	inode_ctldir->i_mode |= (S_IFDIR | S_IRWXU);
	inode_ctldir->i_op = &zfsctl_dir_inode_operations;
	inode_ctldir->i_fop = &zfsctl_dir_file_operations;
	ASSERT(vfsp);
	inode_ctldir->i_sb = vfsp->vfs_super;
	ASSERT(vfsp->vfs_super);
	ASSERT(vfsp->vfs_super->s_root);
	unlock_new_inode(inode_ctldir);
	zfsctl_dir_dentry = d_alloc_name(vfsp->vfs_super->s_root, 
					 ZFS_CTLDIR_NAME);
	if (zfsctl_dir_dentry) {
	  d_add(zfsctl_dir_dentry, LZFS_VTOI(vp_zfsctl_dir));
	  vfsp->zfsctl_dir_dentry = zfsctl_dir_dentry;
	} else {
		goto dentry_out;
	}
	set_zfsvfs_ctldir(vfsp->vfs_data, vp_zfsctl_dir);
	mutex_exit(&vp_zfsctl_dir->v_lock);
	inode_snapdir = iget_locked(vfsp->vfs_super, LZFS_ZFSCTL_INO_SNAPDIR);
	ASSERT(inode_snapdir != NULL);
	ASSERT(inode_snapdir->i_state & I_NEW);
	vp_snap_dir = LZFS_ITOV(inode_snapdir);
	gethrestime(&now);
	vfsp->vfs_snap_dir = vp_snap_dir;
	mutex_enter(&vp_snap_dir->v_lock);
	vp_snap_dir->v_count = 1;
	VN_SET_VFS_TYPE_DEV(vp_snap_dir, vfsp, VDIR, 0);
	bcopy(&now,&(vp_snap_dir->v_inode.i_ctime),sizeof (timestruc_t));
	bcopy(&now,&(vp_snap_dir->v_inode.i_atime),sizeof (timestruc_t));
	bcopy(&now,&(vp_snap_dir->v_inode.i_mtime),sizeof (timestruc_t));
	inode_snapdir->i_uid = current->cred->uid;
	inode_snapdir->i_gid = current->cred->gid;
	inode_snapdir->i_version = 1;
	inode_snapdir->i_mode |= (S_IFDIR | S_IRWXU);
	inode_snapdir->i_op = &snap_dir_inode_operations;
	inode_snapdir->i_fop = &snap_dir_file_operations;
	inode_snapdir->i_sb = vfsp->vfs_super;
	unlock_new_inode(inode_snapdir);
	ASSERT(zfsctl_dir_dentry);
	snap_dir_dentry = d_alloc_name(zfsctl_dir_dentry, ZFS_SNAPDIR_NAME);
	if (snap_dir_dentry) {
		d_add(snap_dir_dentry, LZFS_VTOI(vp_snap_dir));
		vfsp->snap_dir_dentry = snap_dir_dentry;
		mutex_exit(&vp_snap_dir->v_lock);
	} else {
		goto dentry_out;
	}
	return;
dentry_out:
	// free vnode
	vn_free(vp_zfsctl_dir);
	ASSERT(0 && "TODO");
}

/*
 * used for cleanups of .zfs and snapshot dirs
 */

void
lzfs_zfsctl_destroy(vfs_t *vfsp)
{
	drop_nlink(LZFS_VTOI(vfsp->vfs_snap_dir));
	mutex_destroy(&(vfsp->vfs_snap_dir->v_lock));
	dput(vfsp->snap_dir_dentry);
	zfsctl_dir_destroy(vfsp->vfs_data);
	dput(vfsp->zfsctl_dir_dentry);
}


