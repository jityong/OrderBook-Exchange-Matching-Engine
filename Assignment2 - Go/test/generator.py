import random
import os
import errno

ops = ["B", "S", "C"]
tickers = ["APPL", "AMZN", "TSLA"]
inputs = []

class Generator:
    def remove(folder):
        if os.path.isdir(folder):
            for filename in os.listdir(folder):
                file_path = os.path.join(folder, filename)
                try:
                    if os.path.isfile(file_path) or os.path.islink(file_path):
                        os.unlink(file_path)
                except Exception as e:
                    print('Failed to delete %s. Reason: %s' % (file_path, e))


    def create_file(filename):
        if not os.path.exists(os.path.dirname(filename)):
            try:
                os.makedirs(os.path.dirname(filename))
            except OSError as exc: # Guard against race condition
                if exc.errno != errno.EEXIST:
                    raise

    # user inputs - number of threads, number of orders
    num_threads = int(input("Enter number of threads: "))
    num_orders = int(input("Enter number of orders: "))

    id = 0
    thread_inputs = {}

    for i in range(num_threads):
        key = f'{i+1}.input'
        thread_inputs[key] = []

    # create random orders
    for i in range(num_orders):
        op = random.choice(ops)
        thread_id = random.randint(1, num_threads)
        order = ''


        if op == "B" or op == "S":
            orderId = id
            ticker = random.choice(tickers)
            count = random.randint(1, 100)
            price = random.randint(2700, 2705)

            order = f'{op} {orderId} {ticker} {price} {count}'

            id+=1

        else: #op == "C":
            orderId = random.randint(0, i)
            order = f'{op} {orderId}'

        # append order to relevant thread and main array
        inputs.append(order)
        key = f'{thread_id}.input'
        thread_inputs[key].append(order)

    # clean input folder
    remove('input')

    # write to input.txt
    filename= './input.txt'
    create_file(filename)
    with open(filename, 'w') as input_file:
        for input in inputs:
            input_file.write("%s\n" % input)
    input_file.close()

    # write the individual threads input files and debug file
    with open('./input-debug.txt', 'w') as input_debug_file:
        for key, thread_input in thread_inputs.items():
            filename = f'input/{key}'
            create_file(filename)
            input_debug_file.write("%s\n" % filename)

            with open(filename, 'w') as input_file:
                for input in thread_input:
                    input_file.write("%s\n" % input)
                    input_debug_file.write("%s\n" % input)
            input_file.close()
            input_debug_file.write("\n")

    input_debug_file.close()