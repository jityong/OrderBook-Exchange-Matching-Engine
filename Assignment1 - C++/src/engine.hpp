// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <vector>

#include "io.h"
#include "hashmap.hpp"

struct Order {
    enum input_type type;
    uint32_t order_id;
    uint32_t price;
    uint32_t count;
    std::string instrument;
    int64_t input_time;
    uint32_t execution_id;
};

struct OrderNode {
    uint32_t price;
    uint32_t volume;
    std::vector<std::shared_ptr<Order>> orders;
    OrderNode *next;
    std::mutex m;

    OrderNode(): price{0}, volume{0}, orders{}, next{nullptr}, m{}{}
    OrderNode(uint32_t price): price{price}, volume{0}, orders{}, next{nullptr}, m{} {}
};

struct BuyBook {
    OrderNode head;
    void add(std::shared_ptr<Order>);
    void matchOrder(std::shared_ptr<Order>);

    BuyBook(): head{OrderNode{}}  {};
};

struct SellBook {
    OrderNode head;
    void add(std::shared_ptr<Order>);
    void matchOrder(std::shared_ptr<Order>);

    SellBook(): head{OrderNode{}} {};
};

class OrderBook {
public:
    void processSellOrder(std::shared_ptr<Order>);
    void processBuyOrder(std::shared_ptr<Order>);
    void processCancelOrder(std::shared_ptr<Order>);
    std::string instrument;
    std::mutex m;

    BuyBook buyBook;
    SellBook sellBook;
    OrderBook(std::string instrument): instrument{instrument}, m{}, buyBook{}, sellBook{} {}
    OrderBook(): instrument{}, m{}, buyBook{}, sellBook{} {}
};


class Engine {
    HashMap<std::string, OrderBook*> orderBooks;
    void ConnectionThread(ClientConnection);
public:
    static HashMap<uint32_t, std::shared_ptr<Order>> orders;
    Engine(): orderBooks{} {};
    void Accept(ClientConnection);
};


inline static std::chrono::microseconds::rep CurrentTimestamp() noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
}

#endif
