#!/usr/bin/env python3

def main():
    print('''
    # When execute order timestamp is not taken when the lock is acquired,
    # then the execution order ids won't match the timestamp order.
    ''')
    print(41)
    print('o')
    print('40 S 8000 GOOG 2700 8000')
    print('w 8000')
    j = 0
    for i in range(200):
        for tid in range(40):
            print(f'{tid} B {j} GOOG 2700 1')
            j += 1
    print('x')

if __name__ == '__main__':
    main()
