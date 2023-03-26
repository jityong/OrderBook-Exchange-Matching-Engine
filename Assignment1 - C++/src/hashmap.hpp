#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include "hashnode.hpp"
#include <functional>
#include <shared_mutex>
#include <mutex>

const size_t DEFAULT_SIZE = 2003;

template<typename K, typename V, typename F = std::hash<K>>
class HashMap {
private:
    size_t size;
    HashNode<K, V> **hashTable;
    std::shared_mutex *bucketMutexes;
    F hashF;

public:
    HashMap(size_t size = DEFAULT_SIZE) : size(size) {
        hashTable = new HashNode<K, V> *[size]();
        bucketMutexes = new std::shared_mutex[size]();
    }

    ~HashMap() {
        for (size_t i = 0; i < size; i++) {
            auto& m = bucketMutexes[i];
            std::unique_lock<std::shared_mutex> lock(m);
            HashNode<K, V> *prev = nullptr;
            HashNode<K, V> *curr = hashTable[i];
            while (curr != nullptr) {
                prev = curr;
                curr = curr->getNext();
                delete prev;
            }
            hashTable[i] = nullptr;
        }

        delete[] bucketMutexes;
        delete[] hashTable;
    };

    // stores the retrieved value of node into provided V argument
    bool get(const K &key, V &value) {
        size_t hash = hashF(key) % size;
        auto& m = bucketMutexes[hash];
        std::shared_lock<std::shared_mutex> lock(m);

        HashNode<K, V> *curr = hashTable[hash];

        while (curr != nullptr) {
            if (curr->getKey() == key) {
                value = curr->getValue();
                return true;
            }
            curr = curr->getNext();
        }
        return false;

    }

    // Keys are not updated
    // if key exist, do nothing
    void put(const K &key, const V &value) {
        size_t hash = hashF(key) % size;
        auto& m = bucketMutexes[hash];
        std::unique_lock<std::shared_mutex> lock(m);

        HashNode<K, V> *prev = nullptr;
        HashNode<K, V> *curr = hashTable[hash];

        while (curr != nullptr && curr->getKey() != key) {
            prev = curr;
            curr = curr->getNext();
        }

        if (curr == nullptr) {
            curr = new HashNode<K, V>(key, value);
            if (prev == nullptr) {
                hashTable[hash] = curr;
            } else {
                prev->setNext(curr);
            }
        }
    }


    void remove(const K &key) {
        size_t hash = hashF(key) % size;
        auto& m = bucketMutexes[hash];
        std::unique_lock<std::shared_mutex> lock(m);

        HashNode<K, V> *prev = nullptr;
        HashNode<K, V> *curr = hashTable[hash];

        while (curr != nullptr && curr->getKey() != key) {
            prev = curr;
            curr = curr->getNext();
        }

        if (curr == nullptr) return;

        if (prev == nullptr) {
            hashTable[hash] = curr->getNext();
        } else {
            prev->setNext(curr->getNext());
        }
        delete curr;
    }
};
#endif //HASHMAP_H
