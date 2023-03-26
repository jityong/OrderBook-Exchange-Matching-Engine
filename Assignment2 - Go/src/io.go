package main

// The cgo code below interfaces with the struct in io.h
// There should be no need to modify this file.

/*
#include <stdint.h>
#include "io.h"

// Capitalized to export.
// Do not use lower caps.
typedef struct {
  enum input_type Type;
  uint32_t Order_id;
  uint32_t Price;
  uint32_t Count;
  char Instrument[9];
}cInput;
*/
import "C"
import (
	"bytes"
	"encoding/binary"
	"fmt"
	"net"
	"unsafe"
)

type input struct {
	orderType  inputType
	orderId    uint32
	price      uint32
	count      uint32
	instrument string
}

type inputType byte

const (
	inputBuy    inputType = 'B'
	inputSell   inputType = 'S'
	inputCancel inputType = 'C'
)

func readInput(conn net.Conn) (in input, err error) {
	buf := make([]byte, unsafe.Sizeof(C.cInput{}))
	_, err = conn.Read(buf)
	if err != nil {
		return
	}

	var cin C.cInput
	b := bytes.NewBuffer(buf)
	err = binary.Read(b, binary.LittleEndian, &cin)
	if err != nil {
		fmt.Printf("read err: %v\n", err)
		return
	}

	in.orderType = (inputType)(cin.Type)
	in.orderId = (uint32)(cin.Order_id)
	in.price = (uint32)(cin.Price)
	in.count = (uint32)(cin.Count)
	in.instrument = C.GoString(&cin.Instrument[0])

	return
}

func outputOrderDeleted(in input, accepted bool, inTime, outTime int64) string {
	acceptedTxt := "A"
	if !accepted {
		acceptedTxt = "R"
	}
	return fmt.Sprintf("X %v %v %v %v\n",
		in.orderId, acceptedTxt, inTime, outTime)
}

func outputOrderAdded(in input, inTime, outTime int64) string {
	//orderType := "S"
	//if in.orderType == inputBuy {
	//	orderType = "B"
	//}
	return fmt.Sprintf("%c %v %v %v %v %v %v\n",
		in.orderType, in.orderId, in.instrument, in.price, in.count, inTime, outTime)
}

func outputOrderExecuted(restingId, newId, execId, price, count uint32, inTime, outTime int64) string {
	return fmt.Sprintf("E %v %v %v %v %v %v %v\n",
		restingId, newId, execId, price, count, inTime, outTime)
}

type cancelSuccessType byte
type MsgType byte

const (
	executeMsg MsgType = 'E'
	cancelMsg = 'C'
	addMsg = 'B'

	acceptCancel cancelSuccessType = 'A'
	rejectCancel = 'R'
)

type Message struct {
	msgType MsgType
	args []interface{}
}

func (m *Message) printMessage() {
	var formatters string
	switch m.msgType {
	case executeMsg:
		formatters = "E %v %v %v %v %v %v %v\n"
	case cancelMsg:
		// formatter for prod
		formatters = "X %v %c %v %v\n"

		// formatter for test
		//formatters = "X %v %v %c %v %v\n"
	case addMsg:
		// formatter for prod
		formatters = "%c %v %v %v %v %v %v\n"

		//formatter for test
		//formatters = "%c %v %v %v %v %v %v %v\n"
	}
	m.args = append(m.args, GetCurrentTimestamp())
	fmt.Printf(formatters, m.args...)
}