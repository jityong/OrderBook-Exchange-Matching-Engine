Tests are split into 3 "difficulty levels"

basic: Single threaded test cases. You should be passing all of these!
intermediate: "Minimally threaded" concurrent test cases, designed to exercise one potentially concurrent execution from very few threads.
hard: "High contention" concurrent test cases, using a very large number of threads to force threads to be scheduled out for long periods of time, and often.

The test cases included are NOT the full set of test cases we will use, but if you pass all of these tests, it should be quite likely that there are little to no bugs left.
