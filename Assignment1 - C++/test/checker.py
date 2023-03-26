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

def processOutputs():
    global output_lines
    dic = {}
    for output in output_lines:
        output = output.strip()
        if not output:
            continue
        if output[0] == '-':
            continue
        curr_output_arr = output.split()
        output_timestamp = int(curr_output_arr[-1])
        dic[output] = output_timestamp
    output_lines = list(filter(lambda x: x.strip() and x[0] != '-', output_lines))
    output_lines.sort(key=lambda x: dic[x])
    print(output_lines)


# sort
# map <output_line> [output_line1, output_line2]

class Order:
    def __init__(self, order_type, order_id, price, instrument, count, input_time):
        self.order_type = order_type
        self.order_id = order_id
        self.price = price
        self.instrument = instrument
        self.count = count
        self.input_time = input_time

    def __lt__(self, other):
        # print("test1")        
        if self.order_type == BUY_TYPE:                
            if self.price == other.price:
                return self.input_time < other.input_time
            return self.price > other.price
        if self.order_type == SELL_TYPE:
            if self.price == other.price:
                return self.input_time < other.input_time
            return self.price < other.price
        print("test")
    
    def __cmp__(self, other):
        return self.type == other.type and self.id == other.id and self.price == other.price and self.instrument == other.instrument and self.count == other.count

def processInputs(input_lines,input_order):
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
            input_order[order_id] = Order(order_type, order_id, price, instrument, count, 0)
        else: 
            continue

def checkCorrect(output_lines,input_order):
    # global input_order
    prev_time = 0
    def removeOrderInPQs(order_id, pqs):    
        for key, pq in pqs.items():        
            for i in range(len(pq)):
                if pq[i].order_id == order_id:
                    pqs[key] = pq[:i] + pq[i+1:]
                    heapq.heapify(pqs[key])
                    return True
        return False
        

    def checkAndUpdateTime(input_time, output_time):
        nonlocal prev_time
        if input_time > output_time or output_time < prev_time:
            print("time got problem")
            return False
        prev_time = output_time
        return True
    # print("cp 1")
    counter = 0
    buybook_pqs = collections.defaultdict(lambda: [])
    sellbook_pqs = collections.defaultdict(lambda: [])
    for output in output_lines:
        output = output.strip()
        print(output)
        if not output:
            continue
        elif output[0] == EXECUTION_TYPE:  
                       
            order_type, resting_id, new_id, execution_id, price, count, input_time, output_time = output.split(" ")
            resting_id, new_id, execution_id, price, count, input_time, output_time = int(resting_id), int(new_id), int(execution_id), int(price), int(count), int(input_time), int(output_time)
            curr_instrument = input_order[resting_id].instrument
            # if not checkAndUpdateTime(input_time, output_time):
            #     return False
            if (input_order[resting_id].order_type == input_order[new_id].order_type):
                print("Matching (execute) both same order type with each other: ", output)
                return False
            resting_order_type = input_order[resting_id].order_type                
            pq = buybook_pqs[curr_instrument] if resting_order_type == BUY_TYPE else sellbook_pqs[curr_instrument]

            # print(curr_instrument, pq[0].order_id)
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
                print("popped from pq order_id: ", pq[0].order_id)
                heapq.heappop(pq)
            #pq[0].count > count
            else:
                pq[0].count -= count

            input_order[new_id].count -= count    
            if input_order[new_id].count < 0:
                print("active order count becomes < 0 after execution of this output", output)
                return False          
            if input_order[new_id].count == 0:
                counter += 1
                print("fully matched order, counter: ", counter)
        elif output[0] == BUY_TYPE or output[0] == SELL_TYPE:
            # print(output)
            counter += 1
            print("Buy/sell counter: ", counter)
            order_type, order_id, instrument, price, count, input_time, output_time = output.split(" ")
            order_id, price, count, input_time, output_time = int(order_id), int(price), int(count), int(input_time), int(output_time)
            # if not checkAndUpdateTime(input_time, output_time):
            #     return False
            curr_order = Order(order_type, order_id, price, instrument, count, input_time)
            
            pq = buybook_pqs[instrument] if output[0] == BUY_TYPE else sellbook_pqs[instrument]

            if curr_order.count != input_order[order_id].count:
                print("Expected count is different. Expected count: ", input_order[order_id].count, output)
                return False                                         

            heapq.heappush(pq, curr_order)      

        elif output[0] == CANCEL_OUTPUT:   
            # print("cp 5")
            # print(output)
            counter += 1
            print("Cancel counter: ", counter)
            op, order_id, status, input_time, output_time = output.split(" ")                
            order_id, input_time, output_time = int(order_id), int(input_time), int(output_time)
            # if not checkAndUpdateTime(input_time, output_time):
            #     return False
            remove_succeed = removeOrderInPQs(order_id, buybook_pqs) or removeOrderInPQs(order_id, sellbook_pqs) 
            if not remove_succeed and status == 'A': 
                print("Cancellation of Order id ", order_id, " should not be accepted")
                return False
            elif remove_succeed and status == 'R': 
                print("Cancellation of Order id ", order_id , " should be accepted")
                return False
        else:
            continue


    # print("cp 6")
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