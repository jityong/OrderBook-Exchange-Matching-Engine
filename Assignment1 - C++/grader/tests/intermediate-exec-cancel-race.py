#!/usr/bin/env python3

def main():
    print('''
    # Adds a pair of matchable concurrent orders, cancels them immediately
    # So the exec may or may not happen depending on if the cancels happen first
    ''')
    print(2)
    print('o')
    j = 0
    for i in range(16000):
        print(f'0 B {j} GOOG 2700 1')
        print(f'0 C {j}')
        j += 1
        print(f'1 S {j} GOOG 2700 1')
        print(f'1 C {j}')
        j += 1
        print(f'0 w {j-1}')
        print(f'1 w {j-2}')
    print('x')

if __name__ == '__main__':
    main()
