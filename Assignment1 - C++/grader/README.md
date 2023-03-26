We created 8 different grader algorithms to accommodate any possible (correct) interpretation of the requirements. These algorithms have been added after manually checking many of the submissions and understanding their interpretation of the assignment writeup.

However, there are still many submissions that fail for all 8 grader algorithms even on our basic test cases with one client, which makes it almost impossible to grade the concurrency requirement of the assignments (about 8 marks out of 20, and 4 marks out of 10, respectively). After manually inspecting many of these submissions, we diagnosed several common mistakes. We are reluctant to modify your submissions since we might introduce bugs that were not initially present.

Some of the mistakes seem minor, and we decided to provide the opportunity for you to fix such bugs for your own submissions. This would help us grade correctness based on the fixes that you provide and would allow us to grade the concurrency component as well.

Some examples of such minor mistakes are:
 - forgetting to flush stdout,
 - incrementing the execution ID incorrectly, or
 - using `buy_price > sell_price` instead of `buy_price >= sell_price`,
 - ... and many many more that we haven't looked at yet.

To see what test cases you are currently failing, please download grader.zip from Luminus, and extract it to a grader subdirectory, next to your code:

path/to/
 - cs3211-a1-.../
   - engine (your engine binary)
   - ... the rest of your C++ code
 - cs3211-a2-.../
   - engine (your engine binary)
   - ... the rest of your Go code
 - grader/
   - Makefile
   - tests/
   - grade_all_testcases.sh
   - grader.cpp
   - ... the rest of the grader.zip

To run the tests, first build the grader:

```
$ cd path/to/grader/
$ make -j
$ # On the lab machines, you may also need to specify the compiler version
$ # make CC=clang-11 CXX=clang++-11 -j
```

After that, you should have a new binary called `grader` in the grader directory.

You can call the grader directly on one test case like so:

```
$ cd path/to/grader
$ ./grader ../cs3211-a1-.../engine < tests/testcasename.in

$ # You can also test the Go engine
$ ./grader ../cs3211-a2-.../engine < tests/testcasename.in

$ # Note that if your engine deadlocks or does not flush output, the grader will wait forever
$ # A detailed explanation of the grader output can be found at the bottom of this README.
```

or you may call the grader on all test cases at once, and log the results into the `logs` directory:

```
$ cd path/to/grader
$ # The exit code is printed at the end of each line
$ # 0 means that test case passed, any other number means it failed.
$ # In this example, the basic and intermediate tests passed but the hard test failed.
$ ./grade_all_testcases.sh ../cs3211-a1-.../engine tests logs
22-04-12-20-31-41,engine,basic-add.in,0
22-04-12-20-31-41,engine,hard-add-add-race.in,1
22-04-12-20-31-41,engine,intermediate-exec.in,0
...

$ # Also works with the Go engine
$ ./grade_all_testcases.sh ../cs3211-a2-.../engine tests logs
```

If you encounter any issues with the grader, please ask on the forum.

If you run the test cases and want to fix minor bugs, please do so **by the end of Reading week, 22 April 2022**:
Please make the least amount of changes possible to fix any issues, and try to pass as many basic test cases as possible (passing all is ideal).
Do not make major changes like rewriting the engine or changing the overall data structure used, unless required for passing the basic test cases.

- make a **new branch** on your Github Classroom repository for Assignment 1 (and/or Assignment 2, if needed)
- push your modifications there
- create a PR named "A1 Resubmission" (or "A2 Resubmission") from the new branch to the main branch.

Please create at most one PR for each assignment. Once you have created a PR, we will regrade your fixed assignment.

----

# Detailed grader output explanation

The grader runs in 2 phases.

## Phase 1: Engine output collection

In the first phase, it sends commands to the engine and collects all output from the engine stdout until no more output is expected.
The engine's stdout and stderr are echoed to the grader's stdout (with prefixes "Engine stderr" and "Engine stdout").
Note that the engine's stderr is ignored for grading purposes, and it's only here for debugging convenience.
Some errors are caught in this phase, such as output parse errors, or if obvious invariants are broken (zero quantity, sending messages about an order that does not exist, incorrect buy/sell price, etc.)

If an error occurs here, the reason is printed and the grader immediately terminates.

```
$ ./grader engines/engine-A0149796E-build-original-4b080ab19bf3255f15d3199522d43706c3ddf1ac < tests/basic-exec-reject-cancel.in
Engine stderr closed
Engine stdout: B 125 GOOG 2705 30 14930384012 14930384097
Engine stdout: E 125 126 1 2705 30 14930384209 14930384215
Engine stdout: S 126 GOOG 2705 0 14930384209 14930384229
Output thread exception: got output for an order that should not exist (it has fully executed, or it was not sent yet)
One or more threads failed
```

In this case, order 126 is fully executed, so "S 126 ..." was unexpected.

The full list of the phase 1 errors is here:

- "Could not parse engine output: ..."
  - An incorrectly formatted line was received, check that you're printing the output messages correctly.
- "got output for an order that should not exist (it has fully executed, or it was not sent yet)"
  - The output we received was unexpected because the order was not active.
- "order does not exist"
  - The order id printed does not and will never exist, perhaps a memory corruption issue?
- "new order does not exist"
  - An execute was printed where the new order id printed does not and will never exist, perhaps a memory corruption issue?
- "filled more than order count"
  - An execute was printed where the new order's quantity dropped below 0.
- "cancel output without corresponding cancel command"
  - A cancel was printed, but we didn't send a cancel request
- "engine closed despite orders still being active"
  - The engine closed stdout, or crashed somehow, but orders were still active, which we expected output messages for.
- "Error reading output"
  - The grader couldn't read from the engine's stdout's file descriptor.

## Phase 2: Validity checking

In the second phase, we try to check if the engine's output was valid. We will try each of the 8 grader algorithms in turn, until one of them passes.
Each algorithm begins with a line indicating what algorithm was used, and then a line will be printed for every output line checked.

The grader contains a single threaded order book, and as each output line is checked, the assignment's requirements are checked first, and if the output line is valid, we will update the state of the grader's order book, and then proceed to check the next output line.

Here is some sample grader output on a student's engine:

```
Checking timestamp order correctness, price-time priority, orders sorted by added-to-book timestamp
Checking: B 0 GOOG 2700 1 20664306272 20664306302
Checking: X 0 A 20664306386 20664306396
Checking: B 2 GOOG 2700 1 20664306273 20664306465
Checking: X 2 A 20664306510 20664306513
...
Checking: B 2214 GOOG 2700 1 20664366606 20664366610
Checking: B 2256 GOOG 2700 1 20664366617 20664366620
Checking: X 2214 A 20664366629 20664366642
Checking: B 2238 GOOG 2700 1 20664366608 20664366643
Checking: E 2238 2189 1 2700 1 20664366631 20664366645
```

If all 8 algorithms fail, then the reason for failure will be printed for each algorithm:

```
timestamp order, price-time priority, orders sorted by added-to-book timestamp error: executed sell order with incorrect resting order, expected one of 2256 but got 2238
timestamp order, price-time priority, orders sorted by input timestamp error: added buy to book when a matchable sell exists
output order, price-time priority, orders sorted by added-to-book timestamp error: rejected cancel for order in book
output order, price-time priority, orders sorted by input timestamp error: rejected cancel for order in book
timestamp order, time priority, orders sorted by added-to-book timestamp error: matched with resting order that was not the earliest matchable order
timestamp order, time priority, orders sorted by input timestamp error: added buy to book when a matchable sell exists
output order, time priority, orders sorted by added-to-book timestamp error: rejected cancel for order in book
output order, time priority, orders sorted by input timestamp error: rejected cancel for order in book
```

Notice that there can be different reasons for failure with different checking algorithms. You should focus on the algorithm that corresponds most closely to your interpretation of the assignment specification.

The 8 different interpretation arise from 3 points, each having 2 possible interpretations:

- Timestamp order vs output order: With timestamp order, the grader will first (stably) sort the engine stdout by output timestamp, before performing any checks. With output order, the engine is responsible for ensuring that the engine stdout is already printed in the correct order.
- Price-time vs time priority: In the original assignment writeup, we described rules for how two orders match, but called it price-time priority. These rules did not correspond to the established definition of price-time priority, a term in the trading industry. In a clarification, we allowed some students to implement the established definition (price-time priority), while others continued to implement the description in the writeup (which we call "time priority" in the grader).
- Orders sorted by added-to-book timestamp vs input timestamp: As part of the rules for matching two orders, an active order must match the "earliest" resting order, if multiple are eligible. In the writeup, we used the word "arrival time", which was ambiguous (arrival into the engine, or arrival into the order book).

This student happened to have the interpretation: "timestamp order, price-time priority, orders sorted by input timestamp". So this student should focus on the following line:

```
timestamp order, price-time priority, orders sorted by input timestamp error: added buy to book when a matchable sell exists
```

They can see the exact offending error (and the output lines leading up to the error) by scrolling up into the "Checking ..." lines:

```
Checking timestamp order correctness, price-time priority, orders sorted by input timestamp
Checking: B 0 GOOG 2700 1 20664306272 20664306302
Checking: X 0 A 20664306386 20664306396
Checking: B 2 GOOG 2700 1 20664306273 20664306465
Checking: X 2 A 20664306510 20664306513
Checking: B 4 GOOG 2700 1 20664306274 20664306555
...
Checking: X 2486 A 20664372229 20664372230
Checking: S 2493 GOOG 2700 1 20664372250 20664372255
Checking: X 2493 A 20664372273 20664372275
Checking: S 2497 GOOG 2700 1 20664372282 20664372285
Checking: B 2518 GOOG 2700 1 20664372316 20664372319
```

So now the error message makes sense. The student's engine added order 2518 to the order book, but resting order 2497 was already in the order book, so it should have executed order 2518 against resting order 2497 instead of adding.

The full list of the phase 2 errors is here:

- "resting order does not exist"
  - For an execute order, the resting order id printed does not and will never exist
- "new order does not exist"
  - For an execute order, the new order id printed does not and will never exist
- "resting order not in book"
  - For an execute order, the resting order id is valid, but not in the order book (it was not added yet, or it was fully filled)
- "new order not active"
  - For an execute order, the active order is not active (already added to the book, fully filled, or not active yet)
- "matched orders for different instruments"
  - For an execute order, the instruments for the new and resting orders are different
- "resting order filled more than initial quantity"
  - For an execute order, the count was too high, and caused the resting order's quantity to drop below 0
- "accepted cancel for order not in book"
  - The engine accepted the cancel when the order was either fully filled, or not added to the book yet
- "rejected cancel for order in book"
  - The engine rejected the cancel when the order was added to the book and still resting
- "cancel order does not exist"
  - The cancel order id does not and will never exist
- "incorrect price"
  - The price printed is not correct. For buy/sell output lines, the price should match the request. For execute output lines, the price should match the resting order.
- "incorrect instrument"
  - We sent a buy/sell request for some instrument, but the instrument name printed by the engine was some other instrument
- "incorrect side"
  - We sent a buy request, but a sell output line was printed (or vice versa)
- "booking fully filled order"
  - The active order already has 0 quantity, so it should not be added to the book
- "booking inactive order"
  - An order was added to the order book
