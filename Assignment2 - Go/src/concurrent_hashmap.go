package main

import (
	"fmt"
	"hash/fnv"
)

const DEFAULT_SIZE int = 2003

type ConcurrentHashMap struct {
	locks         []*RWLock
	numReadersArr []int
	size          int
	hashTable     []*ConcurrentHashNode
}

type RWLock struct {
	turnstile     chan interface{}
	roomEmptyLock chan interface{}
	readSwitch    chan interface{}
	counter       int
}

func NewRWLock() *RWLock {
	return &RWLock{
		turnstile:     make(chan interface{}, 1),
		roomEmptyLock: make(chan interface{}, 1),
		readSwitch:    make(chan interface{}, 1),
		counter:       0,
	}
}

func (rwLock *RWLock) readSwitchLock() {
	rwLock.readSwitch <- struct{}{}
	rwLock.counter += 1
	if rwLock.counter == 1 {
		rwLock.roomEmptyLock <- struct{}{}
	}
	<-rwLock.readSwitch
}

func (rwLock *RWLock) readSwitchUnlock() {
	rwLock.readSwitch <- struct{}{}
	rwLock.counter -= 1
	if rwLock.counter == 0 {
		<-rwLock.roomEmptyLock
	}
	<-rwLock.readSwitch
}

func NewConcurrentHashMap(size ...int) ConcurrentHashMap {
	s := DEFAULT_SIZE
	if len(size) > 0 {
		s = size[0]
	}

	locks := make([]*RWLock, s)
	for i := 0; i < s; i++ {
		locks[i] = NewRWLock()
	}

	return ConcurrentHashMap{
		locks:         locks,
		numReadersArr: make([]int, s),
		size:          s,
		hashTable:     make([]*ConcurrentHashNode, s),
	}
}

func (m *ConcurrentHashMap) getHash(key interface{}) int {
	h := fnv.New32a()
	h.Write([]byte(fmt.Sprint(key)))
	return int(h.Sum32() % uint32(m.size))
}

func (m *ConcurrentHashMap) Get(key interface{}) (interface{}, bool) {
	h := m.getHash(key)
	currLock := m.locks[h]

	currLock.turnstile <- struct{}{}
	<-currLock.turnstile

	currLock.readSwitchLock()

	defer func() {
		currLock.readSwitchUnlock()
	}()

	curr := m.hashTable[h]
	for {
		if curr == nil {
			break
		}

		if curr.getKey() == key {
			return curr.getValue(), true
		}
		curr = curr.next
	}
	return nil, false
}

func (m *ConcurrentHashMap) Set(key interface{}, value interface{}) {
	h := m.getHash(key)
	currLock := m.locks[h]

	currLock.turnstile <- struct{}{}
	currLock.roomEmptyLock <- struct{}{}

	defer func() {
		<-currLock.turnstile
		<-currLock.roomEmptyLock
	}()
	// busy wait
	for {
		if m.numReadersArr[h] == 0 {
			break
		}
	}

	var prev *ConcurrentHashNode
	curr := m.hashTable[h]

	for {
		if curr == nil {
			break
		}

		if curr.getKey() == key {
			return
		}
		prev = curr
		curr = curr.next
	}

	if prev == nil {
		m.hashTable[h] = &ConcurrentHashNode{key, value, nil}
	} else {
		prev.next = &ConcurrentHashNode{key, value, nil}
	}

	return
}

func (m *ConcurrentHashMap) Remove(key interface{}) bool {
	h := m.getHash(key)
	currLock := m.locks[h]

	currLock.turnstile <- struct{}{}
	currLock.roomEmptyLock <- struct{}{}

	defer func() {
		<-currLock.turnstile
		<-currLock.roomEmptyLock
	}()

	// busy wait
	for {
		if m.numReadersArr[h] == 0 {
			break
		}
	}

	var prev *ConcurrentHashNode
	curr := m.hashTable[h]

	for {
		if curr == nil {
			return false
		}

		if curr.getKey() == key {
			break
		}
		prev = curr
		curr = curr.next
	}

	if prev == nil {
		m.hashTable[h] = curr.next
	} else {
		prev.next = curr.next
	}

	return true
}
