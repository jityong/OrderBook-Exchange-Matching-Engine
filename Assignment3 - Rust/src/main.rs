use std::{
    sync::{
        atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering},
        Arc, Condvar, Mutex,
    },
    thread,
    time::Instant,
    iter,
    vec::Vec,
};
use std::sync::RwLock;
use task::{Task, TaskType};
use crossbeam_deque::{Injector, Worker, Steal};

pub struct Counters {
    pub combined_output: AtomicU64,
    pub num_hash: AtomicU64,
    pub num_derive: AtomicU64,
    pub num_rand: AtomicU64,
}

fn main() {
    let (seed, starting_height, max_children) = get_args();

    eprintln!(
        "Using seed {}, starting height {}, max. children {}",
        seed, starting_height, max_children
    );

    let n_workers = num_cpus::get();
    // let n_workers = 1000;

    eprintln!("num cpu: {}", n_workers);

    // let cv = Arc::new((Mutex::new(false), Condvar::new()));
    let initial_queue = Task::generate_initial(seed, starting_height, max_children);
    let queued_count = Arc::new(AtomicUsize::new(initial_queue.len()));
    let num_idle = Arc::new(AtomicUsize::new(n_workers));
    // let num_exit = Arc::new(AtomicUsize::new(0));
    // let should_run = Arc::new(AtomicBool::new(true));
    let counters = Arc::new(Counters {
        combined_output: AtomicU64::new(0),
        num_hash: AtomicU64::new(0),
        num_derive: AtomicU64::new(0),
        num_rand: AtomicU64::new(0),
    });
    let stealers = Arc::new(RwLock::new(Vec::new()));
    let injector = Arc::new(Injector::new());

    for task in initial_queue.into_iter() {
        injector.push(task);
    }
    // start timer
    let start = Instant::now();
    let mut worker_threads = Vec::new();

    for _ in 0..n_workers {
        // clone required arcs
        // let cv_clone = Arc::clone(&cv);
        let num_idle_clone = Arc::clone(&num_idle);
        // let num_exit_clone = Arc::clone(&num_exit);
        // let should_run_clone = Arc::clone(&should_run);
        let queued_count_clone = Arc::clone(&queued_count);
        let counters_clone = Arc::clone(&counters);
        let stealers_clone = Arc::clone(&stealers);
        let injector_clone = Arc::clone(&injector);

        let join_handle = thread::spawn(move || {
            eprintln!("Worker started...");

            let local_queue = &Worker::new_fifo();
            (*stealers_clone.write().unwrap()).push(local_queue.stealer());

            let mut is_idle = true;
            let mut local_output: u64 = 0;
            let mut local_num_hash: u64 = 0;
            let mut local_num_derive: u64 = 0;
            let mut local_num_rand: u64 = 0;

            while true {
                // read from taskq and update num_idle if needed
                // Reference: https://docs.rs/crossbeam-deque/latest/crossbeam_deque/
                let next = local_queue.pop().or_else(|| {
                    // Otherwise, we need to look for a task elsewhere.
                    iter::repeat_with(|| {
                        // Try stealing a batch of tasks from the global queue.
                        let result = injector_clone.steal_batch_and_pop(local_queue)
                            // Or try stealing a task from one of the other threads.
                            .or_else(|| (*stealers_clone.read().unwrap()).iter().map(|s| s.steal()).collect());
                        return result;
                    })
                        // Loop while no task was stolen and any steal operation needs to be retried.
                        .find(|s: &Steal<Task>| !s.is_retry())
                        // Extract the stolen task, if there is one.
                        .and_then(|s| s.success())
                });


                if next.is_some() {
                    if is_idle {
                        is_idle = false;
                        num_idle_clone.fetch_sub(1, Ordering::AcqRel);
                    }
                    queued_count_clone.fetch_sub(1, Ordering::AcqRel);

                    let next_val = next.unwrap();

                    // update count
                    match next_val.typ {
                        TaskType::Hash => local_num_hash += 1,
                        TaskType::Derive => local_num_derive += 1,
                        TaskType::Random => local_num_rand += 1,
                    };

                    let result = next_val.execute();
                    local_output ^= result.0;

                    queued_count_clone.fetch_add(result.1.len(), Ordering::AcqRel);

                    // write into task queues
                    for task in result.1.into_iter() {
                        local_queue.push(task);
                    }
                } else { // no task
                    if !is_idle {
                        is_idle = true;
                        num_idle_clone.fetch_add(1, Ordering::AcqRel);
                    }

                    if queued_count_clone.load(Ordering::Acquire) == 0
                        && num_idle_clone.load(Ordering::Acquire) == n_workers
                    {
                        break;
                    }
                }
            }

            // add local counter and output into global counters and output
            counters_clone.num_hash.fetch_add(local_num_hash, Ordering::Relaxed);
            counters_clone.num_derive.fetch_add(local_num_derive, Ordering::Relaxed);
            counters_clone.num_rand.fetch_add(local_num_rand, Ordering::Relaxed);
            counters_clone.combined_output.fetch_xor(local_output, Ordering::Relaxed);
            eprintln!("thread exiting...");
        });
        worker_threads.push(join_handle);
    }

    for worker in worker_threads.into_iter() {
        worker.join();
    }

    {
        // /* main thread conditional wait */
        // let (lock, cvar) = &*cv;
        // let mut finished = lock.lock().unwrap();
        //
        // // As long as the value inside the `Mutex<bool>` is `false`, we wait.
        // while !*finished {
        //     finished = cvar.wait(finished).unwrap();
        // }
    }

    eprintln!("Main thread woke up!");
    // should_run.store(false, Ordering::Release);

    // while num_exit.load(Ordering::Acquire) != n_workers {
    //     // continue wait until all worker have exit
    //     eprintln!("{}", "i should not be here in num exit waiting");
    // }

    let end = Instant::now();

    eprintln!("Completed in {} s", (end - start).as_secs_f64());

    // read from counters
    println!(
        "{},{},{},{}",
        counters.combined_output.load(Ordering::Acquire),
        counters.num_hash.load(Ordering::Acquire),
        counters.num_derive.load(Ordering::Acquire),
        counters.num_rand.load(Ordering::Acquire),
    );
}

// There should be no need to modify anything below

fn get_args() -> (u64, usize, usize) {
    let mut args = std::env::args().skip(1);
    (
        args.next()
            .map(|a| a.parse().expect("invalid u64 for seed"))
            .unwrap_or_else(|| rand::Rng::gen(&mut rand::thread_rng())),
        args.next()
            .map(|a| a.parse().expect("invalid usize for starting_height"))
            .unwrap_or(5),
        args.next()
            .map(|a| a.parse().expect("invalid u64 for seed"))
            .unwrap_or(5),
    )
}

mod task;


/* main thread busy wait iteration */

// let mut taskq_size = 1;
// let mut curr_num_idle = 0;
// while curr_num_idle != n_workers || taskq_size != 0 {
//     // continue waiting until num_idle = n_workers && taskq is empty
//     // eprintln!(
//     //     "Still waiting! curr_num_idle: {:?}, taskq_size: {:?}", curr_num_idle, taskq_size
//     // );
//     {
//         taskq_size = (*taskq.lock().unwrap()).len();
//         curr_num_idle = num_idle.load(Ordering::SeqCst);
//     }
// }
