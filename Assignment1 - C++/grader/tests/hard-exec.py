#!/usr/bin/env python3

def main():
    print('''
    # Adds many pairs of matchable concurrent orders from different threads, waits for it to exec, and loops
    ''')

    num_pairs = 20
    print(num_pairs * 2)
    print('o')
    j = 0
    for i in range(1600):
        for tid in range(num_pairs):
            print(f'{tid} B {j} INST{tid} 2700 1')
            j += 1
            print(f'{tid+num_pairs} S {j} INST{tid} 2700 1')
            j += 1
            print(f'{tid} w {j-1}')
            print(f'{tid+num_pairs} w {j-2}')
    print('x')

if __name__ == '__main__':
    main()
