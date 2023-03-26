import heapq
import copy
import collections
import operator

BUY_TYPE = "B"
SELL_TYPE = "S"
CANCEL_TYPE = "C"
CANCEL_OUTPUT = "X"
EXECUTION_TYPE = "E"
f1 = open("input.txt", "r")
f2 = open("output.txt", "r")
input_lines = f1.read().splitlines()
output_lines = f2.read().splitlines()
input_order = {}
num_orders = 0
global_execution_id = 1


def processOutputs():
    global output_lines
    def checkIsOutput(c):
        return c == BUY_TYPE or c == SELL_TYPE or c == CANCEL_OUTPUT or c == EXECUTION_TYPE
    dic = {}
    for output in output_lines:
        output = output.strip()
        if not output:
            continue
        if not checkIsOutput(output[0]):
            continue
        curr_output_arr = output.split()
        output_timestamp = int(curr_output_arr[-1])
        dic[output] = output_timestamp
    output_lines = list(filter(lambda x: x.strip() and checkIsOutput(x.strip()[0]), output_lines))
    output_lines.sort(key=lambda x: dic[x])
    print(output_lines)


# sort
# map <output_line> [output_line1, output_line2]

class Order:
    def __init__(self, order_type, order_id, price, instrument, count, input_time = 0, output_time = 0, thread_id=-1):
        self.order_type = order_type
        self.order_id = order_id
        self.price = price
        self.instrument = instrument
        self.count = count
        self.input_time = input_time
        self.output_time = output_time
        self.thread_id = thread_id

    def __lt__(self, other):
        # print("test1")
        if self.order_type == BUY_TYPE:
            if self.price == other.price:
                return self.output_time < other.output_time
            return self.price > other.price
        if self.order_type == SELL_TYPE:
            if self.price == other.price:
                return self.output_time < other.output_time
            return self.price < other.price

    def __cmp__(self, other):
        return self.type == other.type and self.id == other.id and self.price == other.price and self.instrument == other.instrument and self.count == other.count


def processInputs(input_lines, input_order):
    global num_orders
    # global input_order
    for curr in input_lines:
        curr = curr.strip()
        if not curr:
            continue
        elif curr[0] == CANCEL_TYPE:
            num_orders += 1
            order_type, order_id = curr.split(" ")
        elif curr[0] == BUY_TYPE or curr[0] == SELL_TYPE:
            num_orders += 1
            # print("input:", curr)
            order_type, order_id, instrument, price, count = curr.split(" ")
            order_id, price, count = int(order_id), int(price), int(count)
            input_order[order_id] = Order(order_type, order_id, price, instrument, count)
        else:
            continue


def checkCorrect(output_lines, input_order):
    global global_execution_id
    def removeOrderInPQs(order_id, thread_id, pqs):
        for key, pq in pqs.items():
            for i in range(len(pq)):
                if pq[i].order_id == order_id and pq[i].thread_id == thread_id:
                    pqs[key] = pq[:i] + pq[i + 1:]
                    heapq.heapify(pqs[key])
                    return True
        return False

    counter = 0
    buybook_pqs = collections.defaultdict(lambda: [])
    sellbook_pqs = collections.defaultdict(lambda: [])
    for output in output_lines:
        if counter % 10000 == 0:
            print(counter)

        output = output.strip()
#         print(output)
        if not output:
            continue
        elif output[0] == EXECUTION_TYPE:

            order_type, resting_id, new_id, execution_id, price, count, input_time, output_time = output.split(" ")
            resting_id, new_id, execution_id, price, count, input_time, output_time = int(resting_id), int(new_id), int(
                execution_id), int(price), int(count), int(input_time), int(output_time)
            curr_instrument = input_order[resting_id].instrument

            if execution_id != global_execution_id:
                print("Incorrect Execution Id. Expected: ", global_execution_id, " Received: ", execution_id)
                return False
            global_execution_id += 1
            if (input_order[resting_id].order_type == input_order[new_id].order_type):
                print("Matching (execute) both same order type with each other: ", output)
                return False
            resting_order_type = input_order[resting_id].order_type
            pq = buybook_pqs[curr_instrument] if resting_order_type == BUY_TYPE else sellbook_pqs[curr_instrument]

            if (not pq or resting_id != pq[0].order_id):
                if not pq:
                    print("No existing order in pq matched", output)
                else:
                    for order in pq:
                        print(order.order_id)
                    print("order_id: ", pq[0].order_id, "price: ", pq[0].price)
                    print("order_id: ", pq[1].order_id, "price: ", pq[1].price)
                    print("Did not match with best order: ", pq[0].order_id, output)
                return False

            if pq[0].count < count:
                print("pq count less than matched count", output)
                return False
            elif pq[0].count == count:
#                 print("popped from pq order_id: ", pq[0].order_id)
                heapq.heappop(pq)
            else:
                pq[0].count -= count

            input_order[new_id].count -= count
            if input_order[new_id].count < 0:
                print("active order count becomes < 0 after execution of this output", output)
                return False
            if input_order[new_id].count == 0:
                counter += 1
#                 print("fully matched order, counter: ", counter)
        elif output[0] == BUY_TYPE or output[0] == SELL_TYPE:
            # print(output)
            counter += 1
#             print("Buy/sell counter: ", counter)
            order_type, thread_id, order_id, instrument, price, count, input_time, output_time = output.split(" ")
            order_id, price, count, input_time, output_time, thread_id = int(order_id), int(price), int(count), int(
                input_time), int(output_time), int(thread_id)

            curr_order = Order(order_type, order_id, price, instrument, count, input_time, output_time, thread_id)

            pq = buybook_pqs[instrument] if output[0] == BUY_TYPE else sellbook_pqs[instrument]

            if curr_order.count != input_order[order_id].count:
                print("Expected count is different. Expected count: ", input_order[order_id].count, output)
                return False

            heapq.heappush(pq, curr_order)

        elif output[0] == CANCEL_OUTPUT:
            counter += 1
#             print("Cancel counter: ", counter)
            op, thread_id, order_id, status, input_time, output_time = output.split(" ")
            order_id, input_time, output_time, thread_id = int(order_id), int(input_time), int(output_time), int(
                thread_id)

            remove_succeed = removeOrderInPQs(order_id, thread_id, buybook_pqs) or removeOrderInPQs(order_id, thread_id,
                                                                                                    sellbook_pqs)
            if not remove_succeed and status == 'A':
                print("Cancellation of Order id ", order_id, " should not be accepted")
                return False
            elif remove_succeed and status == 'R':
                print("Cancellation of Order id ", order_id, " should be accepted")
                return False
        else:
            continue

    if counter != num_orders:
        print("Some orders didn't get executed, counter: ", counter, "num lines:", len(input_lines))
        return False
    print("success")
    return True


processOutputs()
processInputs(input_lines, input_order)
# print("teststes")
# for order_id, order in input_order.items():
# print(order_id)
if checkCorrect(output_lines, input_order):
    print("Passed")
else:
    print("Failed")
