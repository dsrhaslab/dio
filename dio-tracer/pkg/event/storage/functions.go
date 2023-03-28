package events

// #define _GNU_SOURCE
// #include <fcntl.h>
// #include <linux/stat.h>
// #include <linux/fs.h>
// #include <linux/xattr.h>
import "C"
import (
	"fmt"
)

func GetFileType(file_type uint16) string {
	var file_type_str string
	switch file_type & C.S_IFMT { //bitwise AND to determine file type
	case C.S_IFSOCK:
		file_type_str = "Socket" //socket
	case C.S_IFLNK:
		file_type_str = "Symbolic link" //symbolic link
	case C.S_IFREG:
		file_type_str = "Regular file" //regular file
	case C.S_IFBLK:
		file_type_str = "Block device" //block device
	case C.S_IFDIR:
		file_type_str = "Directory" //directory
	case C.S_IFCHR:
		file_type_str = "Char device" //char device
	case C.S_IFIFO:
		file_type_str = "Pipe" //pipe
	default:
		file_type_str = "Unknown" //unknown
	}
	return file_type_str
}

func GetMknodFType(mode uint16) string {
	var file_type_str, mode_str string
	file_type := mode & C.S_IFMT
	mode_val := mode & (mode &^ C.S_IFMT)
	if val, ok := fileType[file_type]; ok {
		file_type_str = val
	} else {
		// (Zero file type is equivalent to type S_IFREG.)
		if file_type == 0 {
			file_type_str = "S_IFREG"
		} else {
			file_type_str = ""
		}
	}

	mode_str = fmt.Sprintf("%s|%#3o", file_type_str, mode_val)
	return mode_str
}

func getFlagsStr(flags_string string, flags int, map_flags map[int]string) string {
	for key, val := range map_flags {
		if flags&key == key {
			flags = flags &^ key
			if flags_string == "" {
				flags_string = fmt.Sprintf("%v", val)
			} else {
				flags_string = fmt.Sprintf("%s|%v", flags_string, val)
			}
		}
	}
	return flags_string
}

func GetFlags(flags int) string {
	var flags_string string = ""
	for key, val := range fileAccessModes {
		if flags&C.O_ACCMODE == key {
			flags = flags &^ C.O_ACCMODE
			if flags_string == "" {
				flags_string = fmt.Sprintf("%v", val)
			} else {
				flags_string = fmt.Sprintf("%s|%v", flags_string, val)
			}
			break
		}
	}
	return getFlagsStr(flags_string, flags, fileFlags)
}

func GetCreationMode(flags int, mode int) string {
	var mode_string string = ""
	if (flags&C.O_CREAT == C.O_CREAT) || (flags&C.O_TMPFILE == C.O_TMPFILE) {
		mode_string = fmt.Sprintf("%#3o", uint64(mode))
	}
	return mode_string
}

func GetRenameFlags(flags int) string {
	return getFlagsStr("", flags, renameAtFlags)
}

func GetUnlinkAtFlags(flags int) string {
	if flags&C.AT_REMOVEDIR == C.AT_REMOVEDIR {
		return "AT_REMOVEDIR"
	}
	return ""
}

func GetFStatAtFlags(flags int) string {
	return getFlagsStr("", flags, fStatAtFlags)
}

func GetXAttrFlags(flags int) string {
	return getFlagsStr("", flags, xAttrFlags)
}

func GetSeekWhence(whence uint32) string {
	whence_str, ok := seekWhence[whence]
	if !ok {
		return fmt.Sprintf("%d", whence)
	}
	return whence_str
}
