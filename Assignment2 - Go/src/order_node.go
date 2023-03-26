package main

import (
	"container/list"
	"fmt"
	"strings"
)

type Order struct {
	orderType   inputType
	orderId     uint32
	price       uint32
	count       uint32
	instrument  string
	inputTime   int64
	rId         uint32
	executionId uint32
}

func NewOrder(orderType inputType, orderId uint32, price, count uint32, instrument string, inputTime int64, rId uint32, executionId uint32) *Order {
	return &Order{
		orderType:   orderType,
		orderId:     orderId,
		price:       price,
		count:       count,
		instrument:  instrument,
		inputTime:   inputTime,
		rId:         rId,
		executionId: executionId,
	}
}

func (o *Order) SetCount(count uint32) {
	o.count = count
}

func (o *Order) String() string {
	oType := "B"
	if o.orderType == inputSell {
		oType = "S"
	}
	return fmt.Sprintf("		"+"Order: { %s %v x %v @ %v ID: %v RID: %v} \n", oType, o.instrument, o.count, o.price, o.orderId, o.rId)
}

type OrderNode struct {
	price  uint32
	volume uint32
	orders *list.List
	next   *OrderNode
	lock   chan interface{}
}

func newOrderNode1() *OrderNode {
	return &OrderNode{
		price:  0,
		volume: 0,
		orders: list.New(),
		next:   nil,
		lock:   make(chan interface{}, 1),
	}
}

func newOrderNode2(price uint32) *OrderNode {
	return &OrderNode{
		price:  price,
		volume: 0,
		orders: list.New(),
		next:   nil,
		lock:   make(chan interface{}, 1),
	}
}

func NewOrderNode(price ...uint32) *OrderNode {
	if len(price) == 0 {
		return newOrderNode1()
	}
	return newOrderNode2(price[0])
}

func (node *OrderNode) Volume() uint32 {
	return node.volume
}

func (node *OrderNode) Len() int {
	return node.orders.Len()
}

func (node *OrderNode) Head() *list.Element {
	return node.orders.Front()
}

func (node *OrderNode) Remove(e *list.Element) {
	node.volume -= e.Value.(*Order).count
	node.orders.Remove(e)
}

func (node *OrderNode) Update(e *list.Element, updatedOrder *Order) {
	oldOrder := e.Value.(*Order)

	node.volume -= oldOrder.count
	node.volume += updatedOrder.count

	oldOrder.executionId = updatedOrder.executionId
	oldOrder.count = updatedOrder.count
}

func (node *OrderNode) Append(o *Order) {
	node.orders.PushBack(o)
	node.volume += o.count
}

func (node *OrderNode) String() string {
	sb := strings.Builder{}
	iter := node.orders.Front()
	sb.WriteString("		" + fmt.Sprintf("OrderNode: length: %d, price: %v, volume: %v, orders: \n", node.Len(), node.price, node.volume))
	for iter != nil {
		o := iter.Value.(*Order)
		sb.WriteString(o.String())
		iter = iter.Next()
	}
	return sb.String()
}
