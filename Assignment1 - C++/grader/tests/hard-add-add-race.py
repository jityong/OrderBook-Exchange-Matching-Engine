#!/usr/bin/env python3

def main():
    print('''
    # When add order timestamp is not taken when the lock is acquired,
    # then the buys may end up in the linked list
    # in an order that does not match the timestamp order.
    ''')
    print(41)
    print('o')
    j = 0
    for i in range(200):
        for tid in range(40):
            print(f'{tid} B {j} GOOG 2700 1')
            if i == 199:
                print(f'40 w {j}')
            j += 1
    print('40 S 8000 GOOG 2700 8000')
    print('x')

if __name__ == '__main__':
    main()
