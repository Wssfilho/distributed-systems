<<<<<<< HEAD
# Bully Algorithm with MPI

Implementation of the leader election algorithm in distributed systems using MPI (Message Passing Interface).

## Algorithm Description

The Bully Algorithm is used for coordinator election in distributed systems. The process works as follows:

1. **Failure Detection**: When a process detects that the coordinator has failed, it initiates an election.
2. **ELECTION Message**: Sends a message to all processes with the highest ID.
3. **ANSWER Response**: Processes with the highest ID respond and initiate their own elections.
4. **New Coordinator**: The process with the highest ID that does not receive a response becomes the coordinator.
5. **COORDINATOR Announcement**: The new coordinator announces their election to all processes.

### Message Types

- **ELECTION**: Initiates the election process.
- **ANSWER**: Response from processes with the highest ID.
- **COORDINATOR**: Announcement of the new coordinator.

## Compilation and Execution
=======
# Bully Algorithm (Leader Election) with MPI

Implementation of the Bully leader election algorithm for distributed systems using MPI (Message Passing Interface).

## Algorithm Overview

The Bully Algorithm is used to elect a coordinator in a distributed system. The flow is:

1. **Failure detection**: When a process detects that the coordinator is down, it starts an election.
2. **ELECTION message**: It sends an election message to all processes with higher IDs.
3. **OK/ANSWER reply**: Higher-ID processes reply (and may start their own elections).
4. **New coordinator**: The highest-ID alive process eventually becomes the coordinator.
5. **COORDINATOR announcement**: The new coordinator announces itself to everyone.

### Message Types

- **ELECTION**: Starts the election process (in code, this is `TAG_ELEICAO`).
- **OK**: Reply from a higher-ID process (in code, this is `TAG_OK`).
- **COORDINATOR**: Coordinator announcement (in code, this is `TAG_COORDENADOR`).

## Build and Run
>>>>>>> 22ca494 (atualiza comentários e mensagens no código para inglês, melhora a clareza das instruções no Makefile e no arquivo de código-fonte)

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install mpich

# Fedora/RHEL
sudo dnf install mpich mpich-devel

# Arch Linux
sudo pacman -S openmpi

```

<<<<<<< HEAD
### Compile
=======
### Build
>>>>>>> 22ca494 (atualiza comentários e mensagens no código para inglês, melhora a clareza das instruções no Makefile e no arquivo de código-fonte)

```bash
make
```

Or manually:

```bash
mpicc -Wall -O2 -o eleicao eleicao.c
```

### Run
<<<<<<< HEAD

```bash
# Syntax: mpirun -np <N> ./bully_algorithm <process_offline> <process_detector> [process_return]

# Where:

# N: total number of processes
# process_offline: ID of the process that will be dropped (1 to N)
# process_detector: ID of the process that will detect the drop and initiate an election (1 to N)
# process_return: (optional) 1=process returns, 0=does not return (default: 1)

# Example with 5 processes, process 3 offline, process 1 as detector (process (return)
mpirun -np 5 ./bully_algorithm 3 1 1

# Example with 4 processes, process 2 offline, process 4 as detector (process does NOT return)
mpirun -np 4 ./bully_algorithm 2 4 0

# Example with pattern (process returns automatically)
mpirun -np 5 ./bully_algorithm 3 1


## How it Works

1. **Initialization**: All processes start WITHOUT a coordinator
2. **Drop**: The process specified as offline drops after 2 seconds
3. **Detection**: The detector process notices the drop and initiates an election
4. **Election**: Detector process sends ELECTION messages to processes with higher IDs
5. **Responses**: Processes with higher IDs respond OK and initiate their own elections
6. **Coordinator**: The process with the highest active ID becomes the new coordinator
7. **Optional Return**: If configured, the crashed process can return after 9 seconds and force a new election.

## Example Output

```bash
# Running: mpirun -np 5 ./bully_algorithm 3 1
=== Configuration ===
Number of processes: 5
Offline processes: 3
Detecting processes: 1
Crash time: 2.0s
====================

Process 1 initialized (no coordinator)
Process 2 initialized (no coordinator)
Process 3 initialized (no coordinator)
Process 4 initialized (no coordinator)
Process 5 initialized (no coordinator)

--- Process 3 crashed (offline) ---

=== Process 1 detected that process 3 crashed - initiating election ===

Process 1 initiating election (detected crash)
Process 1 -> ELECTION -> Process 2
Process 1 -> ELECTION -> Process 4
Process 1 -> ELECTION -> Process 5

Process 2 received ELECTION from 1
Process 4 received ELECTION from 1
Process 5 received ELECTION from 1

Process 1 received OK from 2
Process 2 starting election (request fulfilled)
...

>>> Process 5 is the new COORDINATOR <<<

=== ELECTION COMPLETED ===
Process 1 - Final Coordinator: 5
Process 2 - Final Coordinator: 5
Process 4 - Final Coordinator: 5
Process 5 - Final Coordinator: 5

```

## Configurable Parameters

In the `bully_algorithm.c` code:

- `timeout`: Wait time for responses (default: 2 seconds)
- Number of processes: Change the `-np` parameter in the `mpirun` command

=======

This project uses process IDs in the range `0..N-1` (same as MPI rank).

```bash
# Syntax: mpirun -np <N> ./eleicao <offline_process> <detector_process> [process_returns]
# Where:
#   N: total number of processes
#   offline_process: ID of the process that will go down (0..N-1)
#   detector_process: ID of the process that will detect the failure and start the election (0..N-1)
#   process_returns: (optional) 1=returns, 0=does not return (default: 1)

# Example with 5 processes: process 2 goes down, process 0 is the detector (process returns)
mpirun -np 5 ./eleicao 2 0 1

# Example with 4 processes: process 1 goes down, process 3 is the detector (process does NOT return)
mpirun -np 4 ./eleicao 1 3 0

# Example using the default (process returns automatically)
mpirun -np 5 ./eleicao 2 0
```

## How It Works (Simulation)

1. **Initialization**: All processes start with no coordinator.
2. **Crash**: The configured offline process goes down after ~2 seconds.
3. **Detection**: The detector process notices the failure and starts an election.
4. **Election**: The detector sends ELECTION messages to higher-ID processes.
5. **Replies**: Higher-ID processes reply with OK and may start their own elections.
6. **Coordinator**: The highest alive process becomes coordinator.
7. **Return (optional)**: If enabled, the failed process returns after ~9 seconds and forces a new election.

## Example Output

```bash
# Running: mpirun -np 5 ./eleicao 2 0
=== Configuration ===
Processes: 5 | Offline: 2 | Detector: 0 | Returns: YES
Times: failure=2.0s | return=9.0s
====================

Process 0 initialized
Process 1 initialized
Process 2 initialized
Process 3 initialized
Process 4 initialized

--- Process 2 crashed (offline) ---

=== Process 0 detected that coordinator process 4 is not responding ===

Process 0 starting election (detected failure)
Process 0 -> ELECTION -> process 1
Process 0 -> ELECTION -> process 3
Process 0 -> ELECTION -> process 4
...

>>> Process 4 is the new COORDINATOR <<<

=== ELECTION COMPLETED ===
Process 0 - Final coordinator: 4
Process 1 - Final coordinator: 4
Process 3 - Final coordinator: 4
Process 4 - Final coordinator: 4
```

>>>>>>> 22ca494 (atualiza comentários e mensagens no código para inglês, melhora a clareza das instruções no Makefile e no arquivo de código-fonte)
## Cleanup

```bash
make clean
```
