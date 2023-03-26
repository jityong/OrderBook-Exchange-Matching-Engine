// There should be no need to modify anything in this file.
use rand::{Rng, RngCore, SeedableRng};

pub type TaskResult = (u64, Vec<Task>);

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum TaskType {
    Hash,
    Derive,
    Random,
}

static TYPE_ARRAY: [TaskType; 3] = [TaskType::Hash, TaskType::Derive, TaskType::Random];

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Task {
    pub typ: TaskType,
    pub seed: u64,
    pub height: usize,
    pub max_children: usize,
}

fn generate_set(seed: u64, height: usize, max_children: usize, max_num: usize) -> Vec<Task> {
    let mut rng = rand_chacha::ChaCha20Rng::seed_from_u64(seed);
    let num_tasks: usize = rng.gen_range(0..=max_num);
    (0..num_tasks)
        .map(|_| Task {
            typ: TYPE_ARRAY[rng.gen_range(0..3)],
            seed: rng.gen(),
            height,
            max_children,
        })
        .collect()
}

impl Task {
    pub fn execute(&self) -> TaskResult {
        let output = match self.typ {
            TaskType::Hash => do_hash(self),
            TaskType::Derive => do_derive(self),
            TaskType::Random => do_random(self),
        };
        (
            output,
            if self.height == 0 {
                Vec::new()
            } else {
                generate_set(
                    self.seed ^ output,
                    self.height - 1,
                    self.max_children,
                    self.max_children,
                )
            },
        )
    }

    pub fn generate_initial(seed: u64, starting_height: usize, max_children: usize) -> Vec<Task> {
        generate_set(seed, starting_height, max_children, 64)
    }
}

fn do_hash(task: &Task) -> u64 {
    let mut rng = rand_chacha::ChaCha20Rng::seed_from_u64(task.seed);
    let rounds: usize = rng.gen_range(0x10000..0x20000);
    let mut state: [u8; 32] = [0; 32];
    rng.fill_bytes(&mut state);

    for _ in 0..rounds {
        let result = ring::digest::digest(&ring::digest::SHA256, &state);
        state.copy_from_slice(result.as_ref());
    }

    let take_from = rng.gen_range(0..24usize);
    u64::from_le_bytes(state[take_from..take_from + 8].try_into().unwrap())
}

fn do_derive(task: &Task) -> u64 {
    let mut rng = rand_chacha::ChaCha20Rng::seed_from_u64(task.seed);
    let mut state: [u8; 64] = [0; 64];
    let mut out: [u8; 64] = [0; 64];
    rng.fill_bytes(&mut state);
    ring::pbkdf2::derive(
        ring::pbkdf2::PBKDF2_HMAC_SHA512,
        rng.gen_range(0x10000..0x20000).try_into().unwrap(),
        &state[..32],
        &state[32..],
        &mut out[..],
    );

    let take_from = rng.gen_range(0..56usize);
    u64::from_le_bytes(out[take_from..take_from + 8].try_into().unwrap())
}

fn do_random(task: &Task) -> u64 {
    let mut rng = rand_chacha::ChaCha20Rng::seed_from_u64(task.seed);
    let rounds: usize = rng.gen_range(0x10000..0x20000);
    for _ in 0..rounds {
        rng.gen::<u64>();
    }
    rng.gen()
}
