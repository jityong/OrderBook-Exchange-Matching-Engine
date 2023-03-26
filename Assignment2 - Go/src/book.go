package main

import (
	"strings"
)

type Book struct {
	head           *OrderNode                        // sorted linkedlist
	sortComparator func(active, resting uint32) bool // comparator used for sorted linkedlist
}

func NewBuyBook() *Book {
	return &Book{
		head: NewOrderNode(),
		sortComparator: func(active, resting uint32) bool {
			// sort resting buy orders from highest to lowest (desc)
			return active > resting
		},
	}
}

func NewSellBook() *Book {
	return &Book{
		head: NewOrderNode(),
		sortComparator: func(active, resting uint32) bool {
			// sort resting buy orders from highest to lowest (desc)
			return active < resting
		},
	}
}

func (b *Book) String() string {
	sb := strings.Builder{}
	iter := b.head
	for iter != nil {
		sb.WriteString("		" + "==================================\n")
		sb.WriteString(iter.String())
		iter = iter.next
	}

	return sb.String()
}

func (b *Book) MatchOrder(o *Order, matchComparator func(i, j uint32) bool) uint32 {
	b.head.lock <- struct{}{}

	v := o.count
	curr := b.head.next
	shouldPopPrev := false
	for curr != nil {
		curr.lock <- struct{}{}

		if shouldPopPrev {
			b.head.next = curr
		}

		if v == 0 || !matchComparator(o.price, curr.price) {
			<-curr.lock
			break
		}

		outputCh := make(chan Message, curr.Len())
		outputChannels <- outputCh
		currVolume := curr.volume

		// execute trades
		go executeOrder(o, v, curr, &outputCh)

		if v >= currVolume {
			// if active order volume is greater than or equal to curr node volume, curr node should be popped
			v -= currVolume
			shouldPopPrev = true
		} else {
			v = 0
			shouldPopPrev = false
		}

		curr = curr.next
	}

	// if reached end of linked list, linked list is empty
	if curr == nil && shouldPopPrev {
		b.head.next = nil
	}

	<-b.head.lock
	return v
}

func executeOrder(o *Order, countLeftToMatch uint32, node *OrderNode, outputCh *chan Message) {
	for node.Len() > 0 && countLeftToMatch != 0 {
		earliestOrderEl := node.Head()
		earliestOrder := earliestOrderEl.Value.(*Order)

		var countMatched uint32
		restingId := earliestOrder.orderId
		matchedPrice := earliestOrder.price
		execution_id := earliestOrder.executionId

		if countLeftToMatch >= earliestOrder.count {
			countMatched = earliestOrder.count
			countLeftToMatch -= earliestOrder.count
			node.Remove(earliestOrderEl)
			orders.Remove(restingId)

		} else {
			countMatched = countLeftToMatch
			updatedOrder := NewOrder(earliestOrder.orderType, earliestOrder.orderId, earliestOrder.price, earliestOrder.count-countLeftToMatch, earliestOrder.instrument, earliestOrder.inputTime, earliestOrder.rId, earliestOrder.executionId + 1)
			node.Update(earliestOrderEl, updatedOrder)
			countLeftToMatch = 0
		}
		*outputCh <- Message{
			executeMsg,
			[]interface{}{restingId, o.orderId, execution_id, matchedPrice, countMatched, o.inputTime},
		}
	}
	close(*outputCh)
	<-node.lock
}

func (b *Book) AddOrder(o *Order, outputCh *chan Message) {
	// book head is already locked
	curr := b.head
	next := curr.next

	// during condition check, holding current & next lock
	// after while loop, only lock the node that we wish to modify( ie. modify orders or update next to new node)
	for next != nil {
		next.lock <- struct{}{}
		if b.sortComparator(o.price, next.price) {
			<-next.lock
			break
		}
		if o.price == next.price {
			<-curr.lock
			curr = next
			break
		}

		<-curr.lock
		curr = next
		next = curr.next
	}

	// case 1: current price != order price
	// case 2: current price == order price
	if o.price != curr.price {
		newNode := NewOrderNode(o.price)
		newNode.lock <- struct{}{}
		newNode.next = curr.next
		curr.next = newNode
		<-curr.lock

		curr = newNode
	}

	curr.Append(o)

	*outputCh <- Message{
		addMsg,
		[]interface{}{o.orderType, o.orderId, o.instrument, o.price, o.count, o.inputTime},
	}
	close(*outputCh)
	<-curr.lock
}

func (b *Book) CancelOrder(o *Order) (err string, ok bool) {
	// book head is already locked
	prev := b.head
	curr := prev.next
	if curr != nil {
		curr.lock <- struct{}{}
	}

	//hand over hand
	for curr != nil && curr.price != o.price {
		next := curr.next
		if next == nil {
			<-prev.lock
			prev = curr
			curr = next
			break
		}
		next.lock <- struct{}{}
		<-prev.lock
		prev = curr
		curr = next
	}

	if curr == nil {
		<-prev.lock
		return "OrderNode not found\n", false
	}

	if curr.price == o.price {
		e := curr.Head()
		for ; e != nil; e = e.Next() {
			if o.orderId == e.Value.(*Order).orderId {
				break
			}
		}

		if e == nil {
			<-prev.lock
			<-curr.lock
			return "Order not found in OrderNode\n", false
		}

		curr.Remove(e)
		orders.Remove(o.orderId)

		if curr.Volume() == 0 {
			prev.next = curr.next
		}
	}

	<-prev.lock
	<-curr.lock
	return "", true
}
