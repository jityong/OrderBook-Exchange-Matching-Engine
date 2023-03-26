#!/usr/bin/env python3

def main():
    print('''
    # When add order timestamp is not taken when the lock is acquired,
    # then the buys may end up in the linked list
    # in an order that does not match the timestamp order.

    # This is an easier version with only 2 buy threads
    ''')
    print(2)
    print('o')
    j = 0
    for i in range(4000):
        for tid in range(2):
            print(f'{tid} B {j} GOOG 2700 1')
            j += 1
    print('0 w {j-1}')
    print('0 S 8000 GOOG 2700 160')
    print('x')

if __name__ == '__main__':
    main()
