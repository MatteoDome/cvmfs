/**
 * This file is part of the CernVM File System.
 *
 * NOTE: when adding or removing public symbols, you must also update the list
 * in libcvmfs_public_syms.txt.
 */

#ifndef CVMFS_LIBCVMFS_H_
#define CVMFS_LIBCVMFS_H_

#define LIBCVMFS_VERSION 2
#define LIBCVMFS_VERSION_MAJOR LIBCVMFS_VERSION
#define LIBCVMFS_VERSION_MINOR 4
// Revision Changelog
// 13: revision introduced
// 14: fix expand_path for absolute paths, add mountpoint to cvmfs_context
// 15: remove counting of open file descriptors
// 16: remove unnecessary free
// 17: apply new classes around the cache manager
// 18: add cvmfs_pread and support for chunked files
// 19: CernVM-FS 2.2.0
// 20: fix reading of chunked files
// 21: CernVM-FS 2.2.3
// 22: CernVM-FS 2.3.0
// 23: update initialization code
#define LIBCVMFS_REVISION 23

#include <sys/stat.h>
#include <unistd.h>

// Legacy error codes
#define LIBCVMFS_FAIL_OK         0
#define LIBCVMFS_FAIL_NOFILES   -1
#define LIBCVMFS_FAIL_MKCACHE   -2
#define LIBCVMFS_FAIL_OPENCACHE -3
#define LIBCVMFS_FAIL_LOCKFILE  -4
#define LIBCVMFS_FAIL_INITCACHE -5
#define LIBCVMFS_FAIL_INITQUOTA -6
#define LIBCVMFS_FAIL_BADOPT    -7


#ifdef __cplusplus
extern "C" {
// Map C++ clases to their C interface names
typedef class LibContext cvmfs_context;
typedef class OptionsManager cvmfs_option_map;  // TODO: rename to cvmfs_options
#else
typedef struct LibContext cvmfs_context;
typedef struct OptionsManager cvmfs_option_map;
#endif

/**
 * Error codes for cvmfs_init_v2.  Mirrors the failure codes in loader.h
 */
typedef enum {
  LIBCVMFS_ERR_OK = 0,
  LIBCVMFS_ERR_UNKNOWN,
  LIBCVMFS_ERR_OPTIONS,
  LIBCVMFS_ERR_PERMISSION,
  LIBCVMFS_ERR_MOUNT,                  // unused in the library
  LIBCVMFS_ERR_LOADER_TALK,            // unused in the library
  LIBCVMFS_ERR_FUSE_LOOP,              // unused in the library
  LIBCVMFS_ERR_LOAD_LIBRARY,           // unused in the library
  LIBCVMFS_ERR_INCOMPATIBLE_VERSIONS,  // unused
  LIBCVMFS_ERR_CACHE_DIR,
  LIBCVMFS_ERR_PEERS,                  // unused
  LIBCVMFS_ERR_NFS_MAPS,
  LIBCVMFS_ERR_QUOTA,
  LIBCVMFS_ERR_MONITOR,                // unused in the library
  LIBCVMFS_ERR_TALK,                   // unused in the library
  LIBCVMFS_ERR_SIGNATURE,
  LIBCVMFS_ERR_CATALOG,
  LIBCVMFS_ERR_MAINTENANCE_MODE,       // unused in the library
  LIBCVMFS_ERR_SAVE_STATE,             // unused in the library
  LIBCVMFS_ERR_RESTORE_STATE,          // unused in the library
  LIBCVMFS_ERR_OTHER_MOUNT,            // unused in the library
  LIBCVMFS_ERR_DOUBLE_MOUNT,           // unused in the library
  LIBCVMFS_ERR_HISTORY,
  LIBCVMFS_ERR_WPAD,
  LIBCVMFS_ERR_LOCK_WORKSPACE,
} cvmfs_errors;


/**
 * Send syslog and debug messages to log_fn instead.  This may (and probably
 * should) be called before any other routine.  Setting this to NULL restores
 * the default logging behavior.
 */
void cvmfs_set_log_fn( void (*log_fn)(const char *msg) );


/**
 * An option map must be created an populated before calling cvmfs_init_v2.
 */
cvmfs_option_map *cvmfs_options_init();
cvmfs_option_map *cvmfs_options_clone(cvmfs_option_map *opts);
void cvmfs_options_fini(cvmfs_option_map *opts);
void cvmfs_options_set(cvmfs_option_map *opts,
                       const char *key, const char *value);
void cvmfs_options_unset(cvmfs_option_map *opts, const char *key);
char *cvmfs_options_get(cvmfs_option_map *opts, const char *key);
void cvmfs_options_free(char *value);


/**
 * Initialize global CVMFS library structures
 * Note: this _must_ be called before anything else in the library.
 *
 * @param[in] options, option1,option2,...
 * \return 0 on success
 */
int cvmfs_init(char const *options);

cvmfs_errors cvmfs_init_v2(cvmfs_option_map *opts);


/**
 * Shut down the CVMFS library and release all resources.  The cvmfs_option_map
 * object passed to cvmfs_init_v2 must be deleted afterwards by a call to
 * cvmfs_options_fini().
 */
void cvmfs_fini();

/**
 * Initialize a CVMFS remote repository.
 *
 * See libcvmfs usage() for possible options.
 *
 * @param[in] options, option1,option2,...
 * \return 0 on success
 */
cvmfs_context* cvmfs_attach_repo(char const *options);

/**
 * Uninitialize a CVMFS remote repository and release all resources for it.
 */
void cvmfs_detach_repo(cvmfs_context *ctx);

/**
 * Load a new catalog if there is one
 * \return 0 on success
 */
int cvmfs_remount(cvmfs_context *ctx);

/**
 * Open a file in the CVMFS cache.
 *
 * @param[in] path, path to open (e.g. /dir/file, not /cvmfs/repo/dir/file)
 * \return read-only file descriptor, -1 on failure (sets errno)
 */
int cvmfs_open(cvmfs_context *ctx, const char *path);

/**
 * Reads from a file descriptor returned by cvmfs_open.  File descriptors that
 * have bit 30 set indicate chunked files.
 */
ssize_t cvmfs_pread(cvmfs_context *ctx,
                    int fd, void *buf, size_t size, off_t off);

/**
 * Closes a file previously opened with cvmfs_open().
 *
 * @param[in] fd, file descriptor to close
 * \return 0 on success, -1 on failure (sets errno)
 */
int cvmfs_close(cvmfs_context *ctx, int fd);

/**
 * Reads a symlink from the catalog.  Environment variables in the
 * symlink that are of the form $(varname) are expanded.  If the
 * provided buffer is not long enough for the null-terminated value of
 * the symlink, -1 is returned and errno is set to ERANGE.
 *
 * @param[in] path, path of symlink (e.g. /dir/file, not /cvmfs/repo/dir/file)
 * @param[out] buf, buffer in which to write the null-terminated value of the symlink
 * @param[in] size, size of buffer
 * \return 0 on success, -1 on failure (sets errno)
 */
int cvmfs_readlink(cvmfs_context *ctx,
  const char *path,
  char *buf,
  size_t size);

/**
 * Get information about a file.  If the file is a symlink,
 * return info about the file it points to, not the symlink itself.
 *
 * @param[in] path, path of file (e.g. /dir/file, not /cvmfs/repo/dir/file)
 * @param[out] st, stat buffer in which to write the result
 * \return 0 on success, -1 on failure (sets errno)
 */
int cvmfs_stat(cvmfs_context *ctx, const char *path, struct stat *st);

/**
 * Get information about a file.  If the file is a symlink,
 * return info about the link, not the file it points to.
 *
 * @param[in] path, path of file (e.g. /dir/file, not /cvmfs/repo/dir/file)
 * @param[out] st, stat buffer in which to write the result
 * \return 0 on success, -1 on failure (sets errno)
 */
int cvmfs_lstat(cvmfs_context *ctx, const char *path, struct stat *st);

/**
 * Get list of directory contents.  The directory contents includes "." and
 * "..".
 *
 * On return, the array will contain a NULL-terminated list of strings.  The
 * caller must free the strings and the array containing them.  The array (*buf)
 * may be NULL when this function is called.
 *
 * @param[in] path, path of directory (e.g. /dir, not /cvmfs/repo/dir)
 * @param[out] buf, pointer to dynamically allocated NULL-terminated array of
 *             strings
 * @param[in] buflen, pointer to variable containing size of array
 * \return 0 on success, -1 on failure (sets errno)
 */
int cvmfs_listdir(
  cvmfs_context *ctx,
  const char *path,
  char ***buf,
  size_t *buflen);

#ifdef __cplusplus
}
#endif

#endif  // CVMFS_LIBCVMFS_H_
