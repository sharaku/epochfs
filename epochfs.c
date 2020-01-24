/*
MIT License

Copyright (c) 2020 Abe Takafumi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

  gcc -Wall epochfs.c `pkg-config fuse --cflags --libs` -o epochfs
*/

#define FUSE_USE_VERSION 29
#define _GNU_SOURCE             /* feature_test_macros(7) �Q�� */

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>


char __base_path[PATH_MAX] = "/tmp/target";
FILE *stream = NULL;
int __epoch_base = 2000;

/*
 * ���ʏ���
 */
#define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
#define EPOCHFS_DEBUG_LOG(fmt, ...) \
			do {										\
				fprintf(stream, "%s: %d:"fmt"\n", __func__, __LINE__, __VA_ARGS__);	\
				fflush(stream);								\
			} while(0)

static void
EPOCHFS_ERROR_LOG(int _errno)
{
	fprintf(stream, "%s: %d: errno=%d (%s)\n",
		__func__, __LINE__, errno, strerror(errno));
	fflush(stream);
}
#else
#define EPOCHFS_DEBUG_LOG(fmt, ...)
static void
EPOCHFS_ERROR_LOG(int _errno) {}
#endif

static inline void
epochfs_mkfullpath(const char *pathname, char *fullpathname)
{
	memcpy(fullpathname, __base_path, PATH_MAX);
	strncat(fullpathname, pathname, PATH_MAX - strlen((const char*)__base_path));
	return;
}

static inline time_t
epochfs_mkepochtime(time_t time)
{
	unsigned long long time_ll = (unsigned long long)time;
	unsigned long long diff_ll;

	int year = __epoch_base - 1970;

	diff_ll = ((((year) * 365 ) + ((year) > 0 ? (((year) + 3) / 4
			- ((year - 1) / 100) + ((year - 1) / 400)) : 0)) * 24 * 3600);
	return (time_t)(time_ll + diff_ll);
}

/*
 * filesystem����
 */
static int
epochfs_statfs(const char *path, struct statvfs *buf)
{
	int rc;
	char fullpath[PATH_MAX];
	epochfs_mkfullpath(path, fullpath);

	EPOCHFS_DEBUG_LOG("path=%s", path);

	rc = statvfs(path, buf);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

/*
 * inode����
 */
static int
epochfs_getattr(const char *pathname, struct stat *buf)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = lstat(fullpathname, buf);
	if (rc < 0) {
		return -errno;
	}

	// epoch���Ԃ����炵�ĉ�������
	buf->st_atime = epochfs_mkepochtime(buf->st_atime);
	buf->st_mtime = epochfs_mkepochtime(buf->st_mtime);
	buf->st_ctime = epochfs_mkepochtime(buf->st_ctime);
	return 0;
}

static int
epochfs_symlink(const char *target, const char *linkpath)
{
	int rc;
	char fulllinkpath[PATH_MAX];
	epochfs_mkfullpath(linkpath, fulllinkpath);

	EPOCHFS_DEBUG_LOG("target=%s linkpath=%s", target, linkpath);

	rc = symlink(target, fulllinkpath);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_readlink(const char *pathname, char *buf, size_t bufsiz)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = readlink(fullpathname, buf, bufsiz);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_mknod(const char *pathname, mode_t mode, dev_t dev)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = mknod(fullpathname, mode, dev);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_mkdir(const char *pathname, mode_t mode)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = mkdir(fullpathname, mode);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_unlink(const char *pathname)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = unlink(fullpathname);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_rmdir(const char *pathname)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = rmdir(fullpathname);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}


static int
epochfs_rename(const char *oldpath, const char *newpath)
{
	int rc;
	char fulloldpath[PATH_MAX];
	char fullnewpath[PATH_MAX];
	epochfs_mkfullpath(oldpath, fulloldpath);
	epochfs_mkfullpath(newpath, fullnewpath);

	EPOCHFS_DEBUG_LOG("oldpath=%s newpath=%s", oldpath, newpath);

	rc = rename(fulloldpath, fullnewpath);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_link(const char *oldpath, const char *newpath)
{
	int rc;
	char fulloldpath[PATH_MAX];
	char fullnewpath[PATH_MAX];
	epochfs_mkfullpath(oldpath, fulloldpath);
	epochfs_mkfullpath(newpath, fullnewpath);

	EPOCHFS_DEBUG_LOG("oldpath=%s newpath=%s", oldpath, newpath);

	rc = link(fulloldpath, fullnewpath);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_chmod(const char *pathname, mode_t mode)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = chmod(fullpathname, mode);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_chown(const char *pathname, uid_t owner, gid_t group)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = chown(fullpathname, owner, group);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_truncate(const char *pathname, off_t length)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = truncate(fullpathname, length);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_utime(const char *pathname, struct utimbuf *times)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = utime(fullpathname, times);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_access(const char *pathname, int mode)
{
	int rc;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = access(fullpathname, mode);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
	int rc;
	char fullpath[PATH_MAX];
	epochfs_mkfullpath(path, fullpath);

	EPOCHFS_DEBUG_LOG("path=%s name=%s value=%s size=%ld flags=%d", path, name, value, size, flags);

	rc = lsetxattr(fullpath, name, value, size, flags);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
	int rc;
	char fullpath[PATH_MAX];
	epochfs_mkfullpath(path, fullpath);

	EPOCHFS_DEBUG_LOG("path=%s name=%s value=%s size=%ld", path, name, value, size);

	rc = lgetxattr(fullpath, name, value, size);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_listxattr(const char *path, char *list, size_t size)
{
	int rc;
	char fullpath[PATH_MAX];
	epochfs_mkfullpath(path, fullpath);

	EPOCHFS_DEBUG_LOG("path=%s list=%p size=%ld", path, list, size);

	rc = llistxattr(fullpath, list, size);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_removexattr(const char *path, const char *name)
{
	int rc;
	char fullpath[PATH_MAX];
	epochfs_mkfullpath(path, fullpath);

	EPOCHFS_DEBUG_LOG("path=%s name=%s", path, name);

	rc = lremovexattr(fullpath, name);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

/*
 * �f�B���N�g������
 */
static int
epochfs_opendir(const char *pathname, struct fuse_file_info *fi)
{
	DIR *dirp;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	dirp = opendir(fullpathname);
	if (dirp == NULL) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	fi->fh = (unsigned long)dirp;
	return 0;
}

static int
epochfs_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler,
	      off_t offset, struct fuse_file_info *fi)
{
	DIR *dirp = (DIR *)fi->fh;
	struct dirent *dirent;
	int ret;

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	for (ret = 0, dirent = readdir(dirp); dirent != NULL && ret == 0; dirent = readdir(dirp)) {
		ret = filler(buf, dirent->d_name, NULL, 0);
	}

	return 0;
}

static int
epochfs_releasedir(const char *pathname, struct fuse_file_info *fi)
{
	int rc;
	DIR *dirp = (DIR *)fi->fh;

	EPOCHFS_DEBUG_LOG("pathname=%s", pathname);

	rc = closedir(dirp);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

/*
 * �t�@�C������
 */
static int
epochfs_open(const char *pathname, struct fuse_file_info *fi)
{
	int fd;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s flags=0x%08X", pathname, fi->flags);

	fd = open(fullpathname, fi->flags);
	if (fd < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	fi->fh = (unsigned long)fd;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d", pathname, fd);
	return 0;
}

static int
epochfs_create(const char *pathname, mode_t mode, struct fuse_file_info *fi)
{
	int fd;
	char fullpathname[PATH_MAX];
	epochfs_mkfullpath(pathname, fullpathname);

	EPOCHFS_DEBUG_LOG("pathname=%s flags=0x%08X", pathname, fi->flags);

	fd = creat(fullpathname, mode);
	if (fd < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	fi->fh = (unsigned long)fd;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d", pathname, fd);
	return 0;
}

static int
epochfs_read(const char *pathname, char *buf, size_t count, off_t offset,
	     struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	ssize_t ret;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d", pathname, fd);

	ret = pread(fd, (void*)buf, count, offset);
	if (ret < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return ret;
}

static int
epochfs_write(const char *pathname, const char *buf, size_t count, off_t offset,
	     struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	ssize_t ret;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d", pathname, fd);

	ret = pwrite(fd, (void*)buf, count, offset);
	if (ret < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return ret;
}

static int
epochfs_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d datasync=%d", pathname, fd, datasync);

	if (datasync) {
		rc = fdatasync(fd);
		if (rc < 0) {
			EPOCHFS_ERROR_LOG(errno);
			return -errno;
		}
	} else {
		rc = fsync(fd);
		if (rc < 0) {
			EPOCHFS_ERROR_LOG(errno);
			return -errno;
		}
	}
	return 0;
}

static int
epochfs_flush(const char *pathname, struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d", pathname, fd);

	rc = fdatasync(fd);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	rc = fsync(fd);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_ftruncate(const char *pathname, off_t length, struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d length=%ld", pathname, fd, length);

	rc = ftruncate(fd, length);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_fgetattr(const char *pathname, struct stat *buf, struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d buf=%p", pathname, fd, buf);

	rc = fstat(fd, buf);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}

	// epoch���Ԃ����炵�ĉ�������
	buf->st_atime = epochfs_mkepochtime(buf->st_atime);
	buf->st_mtime = epochfs_mkepochtime(buf->st_mtime);
	buf->st_ctime = epochfs_mkepochtime(buf->st_ctime);
	return 0;
}

static int
epochfs_flock(const char *pathname, struct fuse_file_info *fi, int op)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d op=%d", pathname, fd, op);

	rc = flock(fd, op);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_fallocate(const char *pathname, int mode, off_t offset, off_t len, struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d mode=%d offset=%ld len=%ld", pathname, fd, mode, offset, len);

	rc = fallocate(fd, mode, offset, len);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_lock(const char *pathname, struct fuse_file_info *fi, int cmd, struct flock *fl)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d cmd=%d fl=%p", pathname, fd, cmd, fl);

	rc = fcntl(fd, cmd, fl);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	return 0;
}

static int
epochfs_release(const char *pathname, struct fuse_file_info *fi)
{
	int fd = (int)fi->fh;
	int rc;

	EPOCHFS_DEBUG_LOG("pathname=%s fd=%d", pathname, fd);

	rc = close(fd);
	if (rc < 0) {
		EPOCHFS_ERROR_LOG(errno);
		return -errno;
	}
	fi->fh = (unsigned long)-1;
	return 0;
}

static struct fuse_operations epochfs_ope = {
	// super operations
//	.init		= ,
//	.destroy	= ,
	.statfs		= epochfs_statfs,

	// inode operations
	.getattr	= epochfs_getattr,
	.access		= epochfs_access,
	.opendir	= epochfs_opendir,
	.readdir	= epochfs_readdir,
	.releasedir	= epochfs_releasedir,
	.readlink	= epochfs_readlink,
	.mknod		= epochfs_mknod,
	.mkdir		= epochfs_mkdir,
	.symlink	= epochfs_symlink,
	.unlink		= epochfs_unlink,
	.rmdir		= epochfs_rmdir,
	.rename		= epochfs_rename,
	.link		= epochfs_link,
	.chmod		= epochfs_chmod,
	.chown		= epochfs_chown,
	.truncate	= epochfs_truncate,
//	.utimens	= ,
	.utime		= epochfs_utime,
	.setxattr	= epochfs_setxattr,
	.getxattr	= epochfs_getxattr,
	.listxattr	= epochfs_listxattr,
	.removexattr	= epochfs_removexattr,

	// file operations
	.open		= epochfs_open,
	.create		= epochfs_create,
	.flush		= epochfs_flush,
	.fsync		= epochfs_fsync,
	.read		= epochfs_read,
	.write		= epochfs_write,
	.ftruncate	= epochfs_ftruncate,
	.fgetattr	= epochfs_fgetattr,
	.flock		= epochfs_flock,
	.fallocate	= epochfs_fallocate,
	.lock		= epochfs_lock,
//	.ioctl		= ,
//	.poll		= ,
	.release	= epochfs_release,

	// address space operations
//	.bmap		= ,
//	.write_buf	= ,
//	.read_buf	= ,

	// getdir�͌Â��C���^�t�F�[�X�B
	.getdir		= NULL,
	// fsyncdir�͕s�v�B
	.fsyncdir	= NULL,
};

int main(int argc, char *argv[])
{
#ifdef DEBUG_ENABLE
	do {
		int i;
		stream = fopen("/media/share/epochfs/log.txt", "a");
		EPOCHFS_DEBUG_LOG("start argc=%d", argc);
		for (i = 0; i < argc; i++) {
			EPOCHFS_DEBUG_LOG(" args[%d]=%s", i, argv[i]);
		}
	} while(0);
#endif

	return fuse_main(argc, argv, &epochfs_ope, NULL);
}
