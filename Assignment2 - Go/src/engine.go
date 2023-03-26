package main

import "C"
import (
	"context"
	"fmt"
	"io"
	"net"
	"os"
	"time"
)

// Global variables
var id uint32 = 0
var idLock = make(chan interface{}, 1)
var orders = NewConcurrentHashMap()
var ordersLock = make(chan interface{}, 1)
var orderbooks = NewConcurrentHashMap()
var outputChannels = make(chan chan Message, 100)

type Engine struct{}

func (e *Engine) accept(ctx context.Context, conn net.Conn) {
	go func() {
		<-ctx.Done()
		conn.Close()
	}()
	go handleConn(conn, ctx)
}

func printOutputChnl(ctx context.Context) {
	for {
		select {
		case ch := <-outputChannels:
		innerLoop:
			for {
				select {
				case s, ok := <-ch:
					if !ok {
						break innerLoop
					}
					s.printMessage()
				case <-ctx.Done(): // should exit
					return
				}
			}

		case <-ctx.Done(): // should exit
			return
		}
	}
}

func handleConn(conn net.Conn, ctx context.Context) {
	// obtain go routine id for current connection
	idLock <- struct{}{}
	rId := id
	id++
	<-idLock

	defer conn.Close()
	for {
		in, err := readInput(conn)
		if err != nil {
			if err != io.EOF {
				_, _ = fmt.Fprintf(os.Stderr, "Error reading input: %v\n", err)
			}
			return
		}
		timeNow := GetCurrentTimestamp()

		switch in.orderType {
		case inputCancel:
			//fmt.Printf("ThreadId: %v, Got cancel ID: %v\n", rId, in.orderId)

			outputCh := make(chan Message, 1)
			// Locking orders here prevent concurrent Add op from racing with the Get()
			ordersLock <- struct{}{}
			if val1, ok := orders.Get(in.orderId); !ok {
				//fmt.Printf("Cancel rejected: Order not found in order map\n")
				outputChannels <- outputCh
				<-ordersLock
				outputCh <- Message{
					cancelMsg,
					[]interface{}{in.orderId, rejectCancel, timeNow},
				}
				close(outputCh)
			} else {
				<-ordersLock
				o := val1.(*Order)
				if o.rId != rId {
					//fmt.Printf("Cancel rejected, Goroutine rId: %d, Order rId: %d\n", rId, o.rId)
					outputChannels <- outputCh
					outputCh <- Message{
						cancelMsg,
						[]interface{}{in.orderId, rejectCancel, timeNow},
					}
					close(outputCh)
					break
				}
				close(outputCh)
				val2, _ := orderbooks.Get(o.instrument)
				ob := val2.(*OrderBook)

				ob.ordersChn <- &Event{o, inputCancel}
			}
		default:
			val, ok := orderbooks.Get(in.instrument)

			if !ok {
				orderbooks.Set(in.instrument, NewOrderBook(in.instrument, ctx))
				val, ok = orderbooks.Get(in.instrument)

				if !ok {
					fmt.Printf("Ooops! Insertion into ConcurrentHashMap failed")
				}
			}
			ob := val.(*OrderBook)

			//fmt.Printf("ThreadId: %v, Got order: %c %v x %v @ %v ID: %v\n",
			//	rId, in.orderType, in.instrument, in.count, in.price, in.orderId)

			o := NewOrder(in.orderType, in.orderId, in.price, in.count, in.instrument, timeNow, rId, 1)
			ob.ordersChn <- &Event{o, o.orderType}
		}
	}
}

func GetCurrentTimestamp() int64 {
	return time.Now().UnixMicro()
}
