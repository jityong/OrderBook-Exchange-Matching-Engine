import subprocess
import random
import os


print("Number of times to run each test: ")
num_runs_per_test = int(input())

base_folder = "../seq_exec"
base_folder_result_path = base_folder + "/result.txt"
current_result_path = "result.txt"
current_folder = "src"
commands = []
base_test_results = []

def generate_command(input_seed, starting_height, max_child):
    return "cargo run -r " + str(input_seed) + " " + str(starting_height) + " " + str(max_child)

def generate_results(cwd_folder, commands, num_runs_per_test):
    results = []  
    for command in commands:
        for i in range(num_runs_per_test):
            p = subprocess.Popen(command, cwd=cwd_folder, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            p.wait()

            result = p.stdout.readline().decode("utf-8").strip()
            time_taken = 0
            for line in p.stderr:
                line_arr = line.split()
                if line_arr and line_arr[0] == b'Completed':
                    time_taken = float(line_arr[2])
                    break
            print("Generating results: ", result + " " + str(time_taken) + " " + command)
            results.append((result, time_taken, command))
    return results

def compare_results(base_results, current_results):
    wrong_results = []
    perf_diff = []
    base_results_idx = 0
    for i in range(len(current_results)):
        if i != 0 and i % num_runs_per_test == 0:
            base_results_idx += 1
            
        if base_results[base_results_idx][0] != current_results[i][0]:
            wrong_results.append("Expected: " + base_results[base_results_idx][0] + " Got: " + current_results[i][0] + " for command: " + current_results[i][2])

        perf_diff.append(base_results[base_results_idx][1] / current_results[i][1])
    print("perf_diff: ", perf_diff)
    result_perf_diff = sum(perf_diff) / len(perf_diff)

    return (wrong_results, result_perf_diff)  

def write_results_to_file(results, file_path):
    f = open(file_path, "w")
    for result in results:
        write_str = str(result[0]) + " " + str(result[1]) + " " + str(result[2] + "\n")
        f.write(write_str)

f = None
if not os.path.exists(base_folder_result_path):
    f = open(base_folder_result_path, "w+")
else:
    f = open(base_folder_result_path, "r")
    
prev_base_results = f.readlines()
print("prev_base_results: ", prev_base_results)
if prev_base_results:
    print("is not empty")
    for result in prev_base_results:        
        result_arr = result.split()        
        current_command = generate_command(result_arr[-3], result_arr[-2], result_arr[-1])
        base_test_results.append((result_arr[0], float(result_arr[1]), current_command))
        commands.append(current_command)
else:
    print("Existing base_result does not exists. Enter number of seeds to test: ")
    num_tests = int(input())

    print("Starting height: ")
    starting_height = int(input())

    print("Max Child: ")
    max_child = int(input())
    
    # seeds = [5664168989938163334, 1976915708242608314, 12605174704058567923]
    # seeds = [12605174704058567923]
    seeds = []
    for i in range(num_tests):
        seeds.append(random.getrandbits(64))  

    for seed in seeds:
        commands.append(generate_command(seed, starting_height, max_child))    
        
    processes_base_test = []
    print("Generating results for base test")
    base_test_results = generate_results(base_folder, commands, 1)
    
    write_results_to_file(base_test_results, base_folder_result_path)
    
print("Generating results for current test")
print("commands: ", commands)
current_test_results = generate_results(current_folder, commands, num_runs_per_test)
write_results_to_file(current_test_results, current_result_path)

compared_results = compare_results(base_test_results, current_test_results)

if len(compared_results[0]) > 0:
    print("Wrong results: ")
    for i, r in enumerate(compared_results[0]):
        print(str(i) + ": " + r)
else:
    print("Success")

print("Speed up by magnitude: ", compared_results[1])