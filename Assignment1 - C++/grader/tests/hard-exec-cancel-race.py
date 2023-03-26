#!/usr/bin/env python3

def main():
    print('''
    # Adds a pair of matchable concurrent orders, cancels them immediately
    # So the exec may or may not happen depending on if the cancels happen first
    ''')
    num_pairs = 20
    print(num_pairs * 2)
    print('o')
    j = 0
    for i in range(1600):
        for tid in range(num_pairs):
            print(f'{tid} B {j} GOOG 2700 1')
            print(f'{tid} C {j}')
            j += 1
            print(f'{tid+num_pairs} S {j} GOOG 2700 1')
            print(f'{tid+num_pairs} C {j}')
            j += 1
            print(f'{tid} w {j-1}')
            print(f'{tid+num_pairs} w {j-2}')
    print('x')

if __name__ == '__main__':
    main()
