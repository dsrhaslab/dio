package events

// #define _GNU_SOURCE
// #include <fcntl.h>
// #include <linux/stat.h>
// #include <linux/fs.h>
// #include <linux/xattr.h>
import "C"

var fileFlags = map[int]string{
	C.O_CREAT:    "O_CREAT",
	C.O_EXCL:     "O_EXCL",
	C.O_NOCTTY:   "O_NOCTTY",
	C.O_TRUNC:    "O_TRUNC",
	C.O_APPEND:   "O_APPEND",
	C.O_NONBLOCK: "O_NONBLOCK",
	C.O_SYNC:     "O_SYNC",
	C.O_DSYNC:    "O_DSYNC",
	C.O_DIRECT:   "O_DIRECT",
	// C.O_LARGEFILE: "O_LARGEFILE",
	C.O_NOFOLLOW:  "O_NOFOLLOW",
	C.O_NOATIME:   "O_NOATIME",
	C.O_CLOEXEC:   "O_CLOEXEC",
	C.O_PATH:      "O_PATH",
	C.O_TMPFILE:   "O_TMPFILE",
	C.O_DIRECTORY: "O_DIRECTORY",
	C.O_ASYNC:     "O_ASYNC",
}

var fileAccessModes = map[int]string{
	C.O_RDONLY: "O_RDONLY",
	C.O_WRONLY: "O_WRONLY",
	C.O_RDWR:   "O_RDWR",
}

var fileCreationModes = map[int]string{
	C.S_IRWXU: "S_IRWXU", /*  0000700: RWX mask for owner */
	C.S_IRUSR: "S_IRUSR", /*  0000400: R for owner */
	C.S_IWUSR: "S_IWUSR", /*  0000200: W for owner */
	C.S_IXUSR: "S_IXUSR", /*  0000100: X for owner */

	C.S_IRWXG: "S_IRWXG", /*  0000070: RWX mask for group */
	C.S_IRGRP: "S_IRGRP", /*  0000040: R for group */
	C.S_IWGRP: "S_IWGRP", /*  0000020: W for group */
	C.S_IXGRP: "S_IXGRP", /*  0000010: X for group */

	C.S_IRWXO: "S_IRWXO", /*  0000007: RWX mask for other */
	C.S_IROTH: "S_IROTH", /*  0000004: R for other */
	C.S_IWOTH: "S_IWOTH", /*  0000002: W for other */
	C.S_IXOTH: "S_IXOTH", /*  0000001: X for other */

	C.S_ISUID: "S_ISUID", /*  0004000: set user id on execution */
	C.S_ISGID: "S_ISGID", /*  0002000: set group id on execution */
	C.S_ISVTX: "S_ISVTX", /*  0001000: save swapped text even after use */
}

var renameAtFlags = map[int]string{
	C.RENAME_NOREPLACE: "RENAME_NOREPLACE",
	C.RENAME_EXCHANGE:  "RENAME_EXCHANGE",
	C.RENAME_WHITEOUT:  "RENAME_WHITEOUT",
}
var fStatAtFlags = map[int]string{
	C.AT_EMPTY_PATH:       "EMPTY_PATH",
	C.AT_NO_AUTOMOUNT:     "NO_AUTOMOUNT",
	C.AT_SYMLINK_NOFOLLOW: "SYMLINK_NOFOLLOW",
}

var xAttrFlags = map[int]string{
	C.XATTR_CREATE:  "XATTR_CREATE",
	C.XATTR_REPLACE: "XATTR_REPLACE",
}

var fileType = map[uint16]string{
	C.S_IFREG:  "S_IFREG",
	C.S_IFCHR:  "S_IFCHR",
	C.S_IFBLK:  "S_IFBLK",
	C.S_IFIFO:  "S_IFIFO",
	C.S_IFSOCK: "S_IFSOCK",
}

var seekWhence = map[uint32]string{
	C.SEEK_SET:  "SEEK_SET",
	C.SEEK_CUR:  "SEEK_CUR",
	C.SEEK_END:  "SEEK_END",
	C.SEEK_DATA: "SEEK_DATA",
	C.SEEK_HOLE: "SEEK_HOLE",
}
