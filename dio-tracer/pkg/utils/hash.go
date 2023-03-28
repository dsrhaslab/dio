package utils

import (
	"bytes"
	"fmt"

	"github.com/pierrec/xxHash/xxHash32"
)

func ComputeXXHash32(msg string) string {
	buf := bytes.NewBufferString(msg)
	hash := fmt.Sprintf("%x", xxHash32.Checksum(buf.Bytes(), 12345))
	return hash
}
