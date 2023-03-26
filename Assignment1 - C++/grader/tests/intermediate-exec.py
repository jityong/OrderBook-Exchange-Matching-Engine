#!/usr/bin/env python3

def main():
    print('''
    # Adds a pair of matchable concurrent orders, waits for it to exec, and loops
    ''')
    print(2)
    print('o')
    j = 0
    for i in range(16000):
        print(f'0 B {j} GOOG 2700 1')
        j += 1
        print(f'1 S {j} GOOG 2700 1')
        j += 1
        print(f'0 w {j-1}')
        print(f'1 w {j-2}')
    print('x')

if __name__ == '__main__':
    main()
