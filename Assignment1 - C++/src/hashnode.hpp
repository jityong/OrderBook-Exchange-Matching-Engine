#ifndef HASHNODE_HPP
#define HASHNODE_HPP

template <typename K, typename V>
class HashNode{
        K key;
        V value;
        HashNode *next;

    public:
        HashNode(const K &key, const V &value) :key(key), value(value), next(nullptr) {}
        ~HashNode(){next = nullptr;}

        K& getKey() {return key;}
        V& getValue() {return value;}
        void setValue(V value) {HashNode::value = value;}
        HashNode *getNext() {return next;}
        void setNext(HashNode *next) {HashNode::next = next;}
};
#endif //HASHNODE_H
