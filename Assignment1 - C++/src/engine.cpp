#include "engine.hpp"

#include <iostream>
#include <thread>

#include "io.h"

HashMap<uint32_t, std::shared_ptr<Order>> Engine::orders{};

void Engine::Accept(ClientConnection connection) {
    std::thread thread{&Engine::ConnectionThread, this,
                       std::move(connection)};
    thread.detach();
}

void Engine::ConnectionThread(ClientConnection connection) {
    while (true) {
        input input;
        switch (connection.ReadInput(input)) {
            case ReadResult::Error:
                std::cerr << "Error reading input" << std::endl;
            case ReadResult::EndOfFile:
                return;
            case ReadResult::Success:
                break;
        }
        int64_t input_time = CurrentTimestamp();
        switch (input.type) {
            case input_cancel:
//                std::cout << "Got cancel: ID: " << input.order_id << std::endl;
//                Output::OrderDeleted(input.order_id, true, input_time,
//                                     CurrentTimestamp());
                break;
            default:
//                std::cout << "Got order: " << static_cast<char>(input.type) << " "
//                          << input.instrument << " x " << input.count << " @ "
//                          << input.price << " ID: " << input.order_id
//                          << std::endl;
                break;
        }

        // executing orders
        std::shared_ptr<Order> order = std::make_shared<Order>(Order{input.type, input.order_id, input.price, input.count,
                    input.instrument, input_time, 1});
        switch (order->type) {
            case input_buy: {
                OrderBook *order_book;
                if (!orderBooks.get(order->instrument, order_book)) {
                    orderBooks.put(order->instrument, new OrderBook{order->instrument});
                    orderBooks.get(order->instrument, order_book);
                }

                order_book->processBuyOrder(order);
                break;
            }
            case input_sell: {
                OrderBook *order_book;
                if (!orderBooks.get(order->instrument, order_book)) {
                    orderBooks.put(order->instrument, new OrderBook{order->instrument});
                    orderBooks.get(order->instrument, order_book);
                }
                order_book->processSellOrder(order);
                break;
            }
            case input_cancel: {
                std::shared_ptr<Order> orderToCancel;
                if (!Engine::orders.get(input.order_id, orderToCancel)) {
                    Output::OrderDeleted(input.order_id, false, input_time, CurrentTimestamp());
                    break;
                }
                OrderBook *order_book;
                orderBooks.get(orderToCancel->instrument, order_book);
                order_book->processCancelOrder(orderToCancel);
                break;
            }
        }
    }

}

void OrderBook::processSellOrder(std::shared_ptr<Order> order) {
    m.lock();

    int v = order->count;

    buyBook.head.m.lock();
    OrderNode *curr = buyBook.head.next;
    while (v > 0 && curr != nullptr) {
        curr->m.lock();
        if (curr->price < order->price) {
            curr->m.unlock();
            break;
        }
        v -= curr->volume;
        curr = curr->next;
    }

    if (v > 0) {
        sellBook.head.m.lock();
    }

    m.unlock();
    buyBook.matchOrder(order);
    if (order->count > 0) {
        sellBook.add(order);
    }
}

void OrderBook::processBuyOrder(std::shared_ptr<Order> order) {
    m.lock();
    int v = order->count;

    sellBook.head.m.lock();
    OrderNode *curr = sellBook.head.next;
    while (v > 0 && curr != nullptr) {
        curr->m.lock();
        if (curr->price > order->price) {
            curr->m.unlock();
            break;
        }
        v -= curr->volume;
        curr = curr->next;
    }
    if (v > 0) {
        buyBook.head.m.lock();
    }

    m.unlock();
    sellBook.matchOrder(order);
    if (order->count > 0) {
        buyBook.add(order);
    }
}

void OrderBook::processCancelOrder(std::shared_ptr<Order> order) {
    OrderNode *head = order->type == input_buy ? &buyBook.head : &sellBook.head;
    head->m.lock();
    OrderNode *curr = head->next;
    if (curr != nullptr) {
        curr->m.lock();
    }
    head->m.unlock();

    //hand overhand
    while (curr != nullptr && curr->price != order->price) {
        OrderNode *next = curr->next;
        if (next == nullptr) {
            curr->m.unlock();
            curr = next;
            break;
        };
        next->m.lock();
        curr->m.unlock();
        curr = next;
    }

    if (curr == nullptr) {
        Output::OrderDeleted(order->order_id, false, order->input_time, CurrentTimestamp());
        return;
    }

    if (curr->price == order->price) {
        auto it = curr->orders.begin();
        for (; it != curr->orders.end(); ++it) {
            if ((*it)->order_id == order->order_id) {
                break;
            }
        }

        if (it == curr->orders.end()) {
            curr->m.unlock();
            Output::OrderDeleted(order->order_id, false, order->input_time, CurrentTimestamp());
            return;
        }

        curr->volume -= order->count;

        curr->orders.erase(it);
        Engine::orders.remove(order->order_id);
        Output::OrderDeleted(order->order_id, true, order->input_time, CurrentTimestamp());
    }
    curr->m.unlock();
}

void BuyBook::add(std::shared_ptr<Order> order) {
    OrderNode *curr = &head;
    OrderNode *next = curr->next;

    // during condition check, holding current & next lock
    // after while loop, only lock the node that we wish to modify( ie. modify orders or update next to new node)
    while (next != nullptr) {
        next->m.lock();
        if (order->price > next->price) {
            next->m.unlock();
            break;
        }
        if (order->price == next->price) {
            curr->m.unlock();
            curr = next;
            break;
        }

        curr->m.unlock();
        curr = next;
        next = curr->next;
    }

    // case 1: current price != order price
    // case 2: current price == order price

    if (order->price != curr->price) {
        OrderNode *newNode = new OrderNode{order->price};
        newNode->m.lock();
        newNode->next = curr->next;
        curr->next = newNode;
        curr->m.unlock();

        curr = newNode;
    }

    auto it = curr->orders.begin();
    for (; it != curr->orders.end() ; ++it) {
        if (order->input_time > (*it)->input_time) {
            break;
        }
    }
    curr->volume += order->count;

    Engine::orders.put(order->order_id, order);
    curr->orders.insert(it, order);
    Output::OrderAdded(order->order_id, order->instrument.c_str(), order->price, order->count,
                       false, order->input_time, CurrentTimestamp());

    curr->m.unlock();

}

void BuyBook::matchOrder(std::shared_ptr<Order> order) {

    OrderNode *curr = &head;
    OrderNode *next = curr->next;
    curr->m.unlock();
    curr = next;
    while (order->count > 0 && curr != nullptr && curr->price >= order->price){
        int numPopback = 0;

        for (auto it = curr->orders.rbegin(); it != curr->orders.rend(); ++it) {
            if (order->count == 0) break;
            uint32_t count_matched;
            uint32_t resting_id = (*it)->order_id;
            uint32_t matched_price = (*it)->price;
            uint32_t current_exec_id = (*it)->execution_id;

            if (order->count >= (*it)->count) {
                count_matched = (*it)->count;
                order->count -= (*it)->count;
                numPopback++;
                Engine::orders.remove(order->order_id);
            } else {
                count_matched = order->count;
                (*it)->count -= order->count;
                (*it)->execution_id += 1;
                order->count = 0;
            }
            curr->volume -= count_matched;

            Output::OrderExecuted(resting_id, order->order_id, current_exec_id, matched_price,
                                  count_matched, order->input_time, CurrentTimestamp());

        }

        for (int i = 0; i < numPopback; i++) {
            curr->orders.pop_back();
        }
        next = curr->next;
        curr->m.unlock();
        curr = next;
    }
}

void SellBook::matchOrder(std::shared_ptr<Order> order) {
    // move out of head
    OrderNode *curr = &head;
    OrderNode *next = curr->next;
    curr->m.unlock();
    curr = next;

    while (order->count > 0 && curr != nullptr && curr->price <= order->price) {
        int numPopback = 0;
        for (auto it = curr->orders.rbegin(); it != curr->orders.rend(); ++it) {
            if (order->count == 0) break;

            uint32_t count_matched;
            uint32_t resting_id = (*it)->order_id;
            uint32_t matched_price = (*it)->price;
            uint32_t current_exec_id = (*it)->execution_id;

            if (order->count >= (*it)->count) {
                count_matched = (*it)->count;
                order->count -= (*it)->count;
                numPopback++;
                Engine::orders.remove(order->order_id);
            } else {
                count_matched = order->count;
                (*it)->count -= order->count;
                (*it)->execution_id += 1;
                order->count = 0;
            }
            curr->volume -= count_matched;

            Output::OrderExecuted(resting_id, order->order_id, current_exec_id, matched_price,
                                  count_matched, order->input_time, CurrentTimestamp());

        }

        for (int i = 0; i < numPopback; i++) {
            curr->orders.pop_back();
        }
        next = curr->next;
        curr->m.unlock();
        curr = next;
    }

}

void SellBook::add(std::shared_ptr<Order> order) {

    OrderNode *curr = &head;
    OrderNode *next = curr->next;

    // during condition check, holding current & next lock
    // after while loop, only lock the node that we wish to modify( ie. modify orders or update next to new node)
    while (next != nullptr) {
        next->m.lock();
        if (order->price < next->price) {
            next->m.unlock();
            break;
        }
        if (order->price == next->price) {
            curr->m.unlock();
            curr = next;
            break;
        }

        curr->m.unlock();
        curr = next;
        next = curr->next;
    }

    // case 1: current price != order price
    // case 2: current price == order price

    if (order->price != curr->price) {
        OrderNode *newNode = new OrderNode{order->price};
        newNode->m.lock();
        newNode->next = curr->next;
        curr->next = newNode;
        curr->m.unlock();

        curr = newNode;
    }


    auto it = begin(curr->orders);
    for (; it != end(curr->orders); ++it) {
        if (order->input_time > (*it)->input_time) {
            break;
        }
    }
    curr->volume += order->count;
    Engine::orders.put(order->order_id, order);
    curr->orders.insert(it, order);
    Output::OrderAdded(order->order_id, order->instrument.c_str(), order->price, order->count,
                       true, order->input_time, CurrentTimestamp());
    curr->m.unlock();
}