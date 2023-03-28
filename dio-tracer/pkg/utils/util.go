package utils

import (
	"bytes"
	"encoding/json"
	"fmt"
	"math/rand"
	"os"
	"reflect"
	"strconv"
	"strings"
	"time"

	"github.com/matishsiao/goInfo"
)

func GetStructType(myvar interface{}) string {

	if t := reflect.TypeOf(myvar); t.Kind() == reflect.Ptr {
		return "*" + t.Elem().Name()
	} else {
		return t.Name()
	}
}

func GetRandomString() string {
	charSet := "abcdedfghijklmnopqrstuvwxyz"
	var random_str strings.Builder
	length := 6
	for i := 0; i < length; i++ {
		random := rand.Intn(len(charSet))
		randomChar := charSet[random]
		random_str.WriteString(string(randomChar))
	}
	return random_str.String()
}

func GetHostName() (string, error) {
	hostname, err := os.Hostname()
	if err != nil {
		return "", fmt.Errorf("failed to get hostname: %s", err)
	}
	return hostname, nil
}

func RemoveElemFromSlice(slice []int, elem uint32) []int {
	index_pos := -1
	for i, target_pid := range slice {
		if elem == uint32(target_pid) {
			index_pos = i
		}
	}
	if index_pos != -1 {
		slice[index_pos] = slice[len(slice)-1]
		return slice[:len(slice)-1]
	}
	return slice
}

func JSONMarshal(t interface{}) ([]byte, error) {
	buffer := &bytes.Buffer{}
	encoder := json.NewEncoder(buffer)
	encoder.SetEscapeHTML(false)
	err := encoder.Encode(t)
	return buffer.Bytes(), err
}

func JSONMarshalIndent(t interface{}) ([]byte, error) {
	return json.MarshalIndent(t, "", "    ")
}

func GenerateSessionID() string {
	var sid strings.Builder
	rand.Seed(time.Now().Unix())
	time_now := time.Now().Format("02.01.2006-15.04.05")
	fmt.Fprintf(&sid, "%s_%s", GetRandomString(), time_now)
	return sid.String()
}

func CheckKernelVersion() bool {

	gi, err := goInfo.GetInfo()
	if err != nil {
		fmt.Println(err)
	}

	vals := strings.Split(gi.Core, ".")

	major, _ := strconv.Atoi(vals[0])
	minor, _ := strconv.Atoi(vals[1])
	if major > 5 || (major == 5 && minor > 4) {
		return false
	}

	return true
}
