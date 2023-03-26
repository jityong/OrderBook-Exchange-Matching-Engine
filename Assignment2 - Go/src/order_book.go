package main

import (
	"context"
	"fmt"
	"strings"
)

type OrderBook struct {
	ordersChn  chan *Event
	ctx        context.Context
	instrument string
	sgl        chan interface{}
	buyBook    *Book
	sellBook   *Book
}

type Event struct {
	order     *Order
	eventType inputType
}

func NewOrderBook(instrument string, ctx context.Context) *OrderBook {
	ordersChn := make(chan *Event, 20)

	orderBook := &OrderBook{
		ordersChn:  ordersChn,
		ctx:        ctx,
		instrument: instrument,
		sgl:        make(chan interface{}, 1),
		buyBook:    NewBuyBook(),
		sellBook:   NewSellBook(),
	}

	go func(ob *OrderBook) {
		for {
			select {
			case o := <-ob.ordersChn:
				switch o.eventType {
				case inputCancel:
					go ob.ProcessCancelOrder(o.order)
				default:
					go ob.ProcessActiveOrder(o.order)
				}
			case <-orderBook.ctx.Done():
				return
			}
		}
	}(orderBook)

	return orderBook
}

func (ob *OrderBook) String() string {
	sb := strings.Builder{}
	sb.WriteString("		" + fmt.Sprintf("OrderBook: instrument: %s \n", ob.instrument))
	sb.WriteString("		" + "1. buyBook: \n")
	sb.WriteString(ob.buyBook.String())
	sb.WriteString("		" + "--------------------------------------------------\n")
	sb.WriteString("		" + "2. sellBook: \n")
	sb.WriteString(ob.sellBook.String())

	return sb.String()
}

func (ob *OrderBook) ProcessActiveOrder(o *Order) {
	var (
		matchBook       *Book
		addBook         *Book
		matchComparator func(activePrice, restingPrice uint32) bool
	)

	if o.orderType == inputBuy {
		matchBook = ob.sellBook
		addBook = ob.buyBook
		matchComparator = func(activePrice, restingPrice uint32) bool {
			// active buy price gte resting sell price
			return activePrice >= restingPrice
		}
	} else {
		matchBook = ob.buyBook
		addBook = ob.sellBook
		matchComparator = func(activePrice, restingPrice uint32) bool {
			// active sell price lte resting buy price
			return activePrice <= restingPrice
		}
	}

	ob.sgl <- struct{}{}

	countLeftToMatch := matchBook.MatchOrder(o, matchComparator)

	if countLeftToMatch > 0 {
		addBook.head.lock <- struct{}{}
		ordersLock <- struct{}{}

		o.SetCount(countLeftToMatch)
		orders.Set(o.orderId, o)

		<-ordersLock

		outputCh := make(chan Message, 1)
		outputChannels <- outputCh

		go func(o *Order, countLeftToAdd uint32, outputCh *chan Message) {
			addBook.AddOrder(o, outputCh)
		}(o, countLeftToMatch, &outputCh)
	}

	<-ob.sgl
}

func (ob *OrderBook) ProcessCancelOrder(o *Order) {
	var book *Book
	if o.orderType == inputBuy {
		book = ob.buyBook
	} else {
		book = ob.sellBook
	}

	book.head.lock <- struct{}{}

	outputCh := make(chan Message, 1)
	outputChannels <- outputCh

	if _, ok := book.CancelOrder(o); !ok {
		//fmt.Printf("Cancel rejected, err: %s\n", errString)
		outputCh <- Message{
			cancelMsg,
			[]interface{}{o.orderId, rejectCancel, o.inputTime},
		}
	} else {
		outputCh <- Message{
			cancelMsg,
			[]interface{}{o.orderId, acceptCancel, o.inputTime},
		}
	}
	close(outputCh)
}
