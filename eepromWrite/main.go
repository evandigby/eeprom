package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"time"

	"github.com/tarm/serial"
)

const (
	CMD_SUCCESS    = 0
	CMD_WRITE      = 1
	CMD_WRITE_PAGE = 2
	CMD_READ       = 3

	CMD_ACK = 199
)

func waitOn(r io.Reader, val byte) {
	readBuf := make([]byte, 1)
	n, err := r.Read(readBuf)
	if err != nil {
		log.Fatal(err)
	}

	if n == 0 {
		log.Fatal("read timeout")
	}

	if readBuf[0] != val {
		log.Fatalf("Error: %v", readBuf[0])
	}
}

func main() {
	c := &serial.Config{Name: "COM4", Baud: 115200, ReadTimeout: time.Second}
	s, err := serial.OpenPort(c)
	if err != nil {
		log.Fatal(err)
	}
	defer s.Close()

	defer s.Flush()

	data := []byte("I am a reasonable sentance.")

	cmd := make([]byte, 4)
	cmd[0] = CMD_READ
	binary.BigEndian.PutUint16(cmd[1:], 0)
	cmd[3] = byte(len(data))

	_, err = s.Write(cmd)
	if err != nil {
		log.Fatal(err)
	}
	s.Flush()

	waitOn(s, CMD_ACK)

	readData := make([]byte, len(data))
	for i := range readData {
		readBuf := make([]byte, 1)
		n, err := s.Read(readBuf)
		if err != nil {
			log.Fatal(err)
		}
		if n == 0 {
			log.Fatal("read timeout")
		}

		readData[i] = readBuf[0]
	}

	waitOn(s, CMD_SUCCESS)

	value := string(readData)
	fmt.Println("Read:", value)
}

// _, err = s.Write([]byte{CMD_WRITE_PAGE})
// if err != nil {
// 	log.Fatal(err)
// }
// fmt.Println(2)

// u16buf := make([]byte, 2)

// binary.BigEndian.PutUint16(u16buf, 0)

// _, err = s.Write(u16buf)
// if err != nil {
// 	log.Fatal(err)
// }
// fmt.Println(3)

// _, err = s.Write([]byte{byte(len(data))})
// if err != nil {
// 	log.Fatal(err)
// }
// fmt.Println(4)

// _, err = s.Write(data)
// if err != nil {
// 	log.Fatal(err)
// }

// response := make([]byte, 1)
// _, err = s.Read(response)
// if err != nil {
// 	log.Fatal(err)
// }
// fmt.Println(6, " - ", int8(response[0]))
