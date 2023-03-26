package main

type ConcurrentHashNode struct {
	K    interface{}
	V    interface{}
	next *ConcurrentHashNode
}

func (n *ConcurrentHashNode) getKey() interface{} {
	return n.K
}

func (n *ConcurrentHashNode) getValue() interface{} {
	return n.V
}

func (n *ConcurrentHashNode) setValue(v interface{}) {
	n.V = v
}

func (n *ConcurrentHashNode) setNext(next *ConcurrentHashNode) {
	n.next = next
}
