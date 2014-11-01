#ifndef __LINUX_MAGIC_H__
#define __LINUX_MAGIC_H__

#define ADFS_SUPER_MAGIC	0xadf5
#define AFFS_SUPER_MAGIC	0xadff
#define AFS_SUPER_MAGIC                0x5346414F
#define AUTOFS_SUPER_MAGIC	0x0187
#define CODA_SUPER_MAGIC	0x73757245
#define CRAMFS_MAGIC		0x28cd3d45	/* some random number */
#define CRAMFS_MAGIC_WEND	0x453dcd28	/* magic number with the wrong endianess */
#define DEBUGFS_MAGIC          0x64626720
#define SECURITYFS_MAGIC	0x73636673
#define SELINUX_MAGIC		0xf97cff8c
#define SMACK_MAGIC		0x43415d53	/* "SMAC" */
#define RAMFS_MAGIC		0x858458f6	/* some random number */
#define TMPFS_MAGIC		0x01021994
#define HUGETLBFS_MAGIC 	0x958458f6	/* some random number */
#define SQUASHFS_MAGIC		0x73717368
#define ECRYPTFS_SUPER_MAGIC	0xf15f
#define EFS_SUPER_MAGIC		0x414A53
#define EXT2_SUPER_MAGIC	0xEF53
#define EXT3_SUPER_MAGIC	0xEF53
#define XENFS_SUPER_MAGIC	0xabba1974
#define EXT4_SUPER_MAGIC	0xEF53
#define BTRFS_SUPER_MAGIC	0x9123683E
#define NILFS_SUPER_MAGIC	0x3434
#define F2FS_SUPER_MAGIC	0xF2F52010
#define HPFS_SUPER_MAGIC	0xf995e849
#define ISOFS_SUPER_MAGIC	0x9660
#define JFFS2_SUPER_MAGIC	0x72b6
#define PSTOREFS_MAGIC		0x6165676C
#define EFIVARFS_MAGIC		0xde5e81e4
#define HOSTFS_SUPER_MAGIC	0x00c0ffee

#define MINIX_SUPER_MAGIC	0x137F		/* minix v1 fs, 14 char names */
#define MINIX_SUPER_MAGIC2	0x138F		/* minix v1 fs, 30 char names */
#define MINIX2_SUPER_MAGIC	0x2468		/* minix v2 fs, 14 char names */
#define MINIX2_SUPER_MAGIC2	0x2478		/* minix v2 fs, 30 char names */
#define MINIX3_SUPER_MAGIC	0x4d5a		/* minix v3 fs, 60 char names */

#define MSDOS_SUPER_MAGIC	0x4d44		/* MD */
#define NCP_SUPER_MAGIC		0x564c		/* Guess, what 0x564c is :-) */
#define NFS_SUPER_MAGIC		0x6969
#define OPENPROM_SUPER_MAGIC	0x9fa1
#define QNX4_SUPER_MAGIC	0x002f		/* qnx4 fs detection */
#define QNX6_SUPER_MAGIC	0x68191122	/* qnx6 fs detection */

#define REISERFS_SUPER_MAGIC	0x52654973	/* used by gcc */
					/* used by file system utilities that
	                                   look at the superblock, etc.  */
#define REISERFS_SUPER_MAGIC_STRING	"ReIsErFs"
#define REISER2FS_SUPER_MAGIC_STRING	"ReIsEr2Fs"
#define REISER2FS_JR_SUPER_MAGIC_STRING	"ReIsEr3Fs"

#define SDCARDFS_SUPER_MAGIC	0x5dca2df5
#define SDFAT_SUPER_MAGIC       0x5EC5DFA4

#define SMB_SUPER_MAGIC		0x517B
#define CGROUP_SUPER_MAGIC	0x27e0eb


#define STACK_END_MAGIC		0x57AC6E9D

#define TRACEFS_MAGIC          0x74726163

#define V9FS_MAGIC		0x01021997

#define BDEVFS_MAGIC            0x62646576
#define BINFMTFS_MAGIC          0x42494e4d
#define DEVPTS_SUPER_MAGIC	0x1cd1
#define FUTEXFS_SUPER_MAGIC	0xBAD1DEA
#define PIPEFS_MAGIC            0x50495045
#define PROC_SUPER_MAGIC	0x9fa0
#define SOCKFS_MAGIC		0x534F434B
#define SYSFS_MAGIC		0x62656572
#define USBDEVICE_SUPER_MAGIC	0x9fa2
#define MTD_INODE_FS_MAGIC      0x11307854
#define ANON_INODE_FS_MAGIC	0x09041934
#define BTRFS_TEST_MAGIC	0x73727279
#define NSFS_MAGIC		0x6e736673

#endif /* __LINUX_MAGIC_H__ */
