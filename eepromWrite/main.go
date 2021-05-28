package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"
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

func waitOn(r io.Reader, val byte) error {
	readBuf := make([]byte, 1)
	n, err := r.Read(readBuf)
	if err != nil {
		return err
	}

	if n == 0 {
		return errors.New("read timeout")
	}

	if readBuf[0] != val {
		return fmt.Errorf("expected %v but got %v", val, readBuf[0])
	}

	return nil
}

func sendCmd(rw io.ReadWriter, cmd byte, address uint16, operand0 byte) error {
	cmdBuf := makeCmd(cmd, address, operand0)
	_, err := rw.Write(cmdBuf)
	if err != nil {
		return err
	}

	return waitOn(rw, CMD_ACK)
}

func readData(rw io.ReadWriter, address uint16, buf []byte) error {
	err := sendCmd(rw, CMD_READ, address, byte(len(buf)))
	if err != nil {
		return err
	}

	i := 0
	for i < len(buf) {
		n, err := rw.Read(buf[i:])
		if err != nil {
			return err
		}
		if n == 0 {
			return errors.New("read timeout")
		}
		i += n
	}

	return waitOn(rw, CMD_SUCCESS)
}

func writePage(rw io.ReadWriter, address uint16, buf []byte) error {
	err := sendCmd(rw, CMD_WRITE_PAGE, address, byte(len(buf)))
	if err != nil {
		return err
	}

	n, err := rw.Write(buf)
	if err != nil {
		return err
	}
	if n < len(buf) {
		return errors.New("read timeout")
	}

	return waitOn(rw, CMD_SUCCESS)
}

func makeCmd(cmd byte, address uint16, operand0 byte) []byte {
	cmdBuf := make([]byte, 4)
	cmdBuf[0] = cmd
	binary.BigEndian.PutUint16(cmdBuf[1:], 0)
	cmdBuf[3] = operand0
	return cmdBuf
}

func main() {
	c := &serial.Config{Name: "COM4", Baud: 115200, ReadTimeout: time.Second}
	s, err := serial.OpenPort(c)
	if err != nil {
		panic(err)
	}
	defer s.Close()

	defer s.Flush()

	data := []byte("I am a big reasonable sentance.")

	err = writePage(s, 0, data)
	if err != nil {
		panic(err)
	}

	buf := make([]byte, len(data))
	err = readData(s, 0, buf)
	if err != nil {
		panic(err)
	}

	fmt.Println(string(buf))
}
