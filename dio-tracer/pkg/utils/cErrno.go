package utils

/*
#include <stdio.h>
#include <string.h>
*/
import "C"

func GetErrorMessage(return_value int64) string {
	cerrstr := C.strerror(C.int(-return_value))
	return C.GoString(cerrstr)
}
