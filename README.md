# **krpsim**

42 school project: read a process/stock description, schedule executable processes under resource
constraints, and output a valid execution trace that either maximizes selected stocks or runs 
until no process is possible within a given time horizon.

---

## **Project Overview**
**krpsim** is a program that ingests a domain-specific text file describing **initial stocks** and **processes**
(with needs, results and durations) and production to **optimize** to simulates a schedule. It prints a
**human-readable trace**. A companion tool, **krpsim_verif**, replays a trace to **validate** that resource
**consumption/production** and **timing** are correct. The codebase is written in **C++** with a simple **Makefile**.

Subject brief: see **krpsim.pdf**.

## **Objectives**
From the subject:
- **Read a configuration file** describing initial stocks, processes and an optimize line.
- Run krpsim <file> <delay> and **compute a “worthy” schedule** under resource constraints (no need to be perfectly optimal).
- **Print a trace** of executed processes with their start cycles.
- Stop when **no more processes** can run (consumable resources exhausted) or, if self-sustaining, use a **reasonable stop condition** showing several process runs.
- Provide krpsim_verif <file> <result_to_test> to **validate a produced trace** and report any error (cycle + process), then show final stocks and last cycle.
---

## **Project Documentation**

[Auto-generated documentation from Docstrings](https://brenaudon.github.io/krpsim/)

---

## **Setup Project**

To set up the project, follow these steps:
1. **Clone the repository:**
   ```bash
    git clone git@github.com:brenaudon/krpsim.git krpsim
    cd krpsim
    ```
2. **Compile:**  


   To compile both programs (krpsim and krpsim_verif), run:
   ```bash
    make
   ```
   To only compile krpsim, run:
   ```bash
    make krpsim
   ```
   To only compile krpsim_verif, run:
   ```bash
    make krpsim_verif
   ```
---

## **Usage**

### **krpsim**

To run the krpsim program, use the following command:
```bash
 ./krpsim <file> <delay>
```
- `<file>`: Path to the input file containing the process and stock description.
- `<delay>`: Time horizon for the simulation (in seconds).

You can find examples of input files in the `configs` directory.

### **krpsim_verif**
To run the krpsim_verif program, use the following command:
```bash
 ./krpsim_verif <file> <result_to_test>
```
- `<file>`: Path to the input file containing the process and stock description.
- `<result_to_test>`: Path to the trace file produced by krpsim to be verified.

## **Implementation**

The implementation of krpsim involves several key components:
- **Parsing**: Reading and interpreting the input file to extract stocks, processes, and optimization targets.
- **Data Structures**: Using appropriate data structures to represent stocks, processes, and the simulation state.
- **Genetic Algorithm**: Implementing a genetic algorithm to tend towards an optimal scheduling of processes.
- **Verification**: Implementing krpsim_verif to validate the correctness of the produced trace.

### **Parsing**

The parsing component reads the input file **line by line**, extracting information about stocks,
processes, and the optimization target. It **handles comments** and ensures that the input **format is correct**.

Regex patterns are used to identify different components:
- Stock definition: `^\s*([^:#\s]+)\s*:\s*(\d+)\s*$` to match stock lines like `stock_name: quantity`
- Process definition: `^\s*([^:]+?)\s*:\s*\(([^)]*)\)\s*:\s*(?:\(([^)]*)\))?\s*:\s*(\d+)\s*$` to match process lines like `<name>:(<need>:<qty>[;<need>:<qty>[...]]):(<result>:<qty>[;<result>:<qty>[...]]):<nb_cycle>`.
Result can be empty as in `Recre` input file example given on intra page of the project.
- Optimize line: `^\s*optimize\s*:\s*\(([^)]*)\)\s*$` to match the optimization target line like `optimize:(<stock_name>|time[;<stock_name>|time[...]])`.

Sections have to **be in order**: stocks, processes, optimize and **all are mandatory**.  
Processes names must be **unique**.  

At the end of parsing, we only keep processes that can lead to the optimization target (i.e. processes that produce stocks that are on the path to the optimization target).
We also detect "obvious" cycles in the process graph. A cycle is considered "obvious" if it is a self-loop or if each process in the cycle produces exactly the needs of the next process in the cycle.

After parsing, the data is stored in appropriate data structures for further processing.

### **Data Structures**

The **main data structure** representing the **config** includes:
- **Initial Stocks**: Represents a stock with its name and quantity.
- **Processes**: Represents a list of processes with their names, needs, results, and duration.
- **Optimization Targets**: Represents the name of what we want to optimize.  

It then includes **members used to optimize** the scheduling:
- **Distance Map**: A map to store the distance of each stock to the optimization target. Used to weigh processes.
- **Maximum Stocks**: An object to store the maximum quantity of each stock really needed to reach the optimization target. Used to better select processes.
- **Items Identification by Ids**: Vectors used to map items to ids and vice versa for quicker accesses.
- **Needers by Item**: A map to store which processes need a particular stock. Used to quickly find processes that can lauch after a stcok increase.

All these members used for optimization are computed once **after parsing** and before the simulation starts.

### **Genetic Algorithm**

The genetic algorithm is implemented to find a near-optimal scheduling of processes under resource constraints. The main steps of the algorithm include:
1. **Initialization**: Generate an initial population of random schedules.
2. **Evaluation**: Evaluate the fitness of each schedule based on the optimization target and resource constraints.
3. **Selection**: Select the best schedules (highest fitness scores) to be parents for the next generation.
4. **Crossover**: Combine pairs of parent schedules to produce offspring schedules.
5. **Mutation**: Introduce random changes to some schedules to maintain genetic diversity.
6. **Replacement**: Replace the old population with the new generation of schedules.
7. **Termination**: Repeat the evaluation, selection, crossover, mutation, and replacement steps until a stopping condition is met (e.g., a maximum number of generations or too much time spent).

#### **Random Schedule Generation**

Random schedules are generated by **randomly selecting processes** to run from the list of available processes
(those that can be executed with the current stock levels) or choosing to **wait** (do nothing) for the next running process to finish.
This is done until the time horizon (50 000 cycles by default) is reached or no more processes can be executed.

#### **Fitness Evaluation**

The fitness of a schedule is evaluated based on the optimization target. The evaluation considers:
- The **final quantity** of the **target** stock(s) at the end of the schedule.
- The **quantity of intermediate stocks**, weighted by their distance to the target stock(s).

If 2 schedules have the **same score**, the one that used **less time** is preferred.

#### **Crossover and Mutations**

Crossover is performed by **parcouring the schedules of the 2 parents** and, at position i, **randomly choosing** 
to take the process from **parent 1 or parent 2** (if it is launchable at this time).
Mutations are performed by randomly choosing to not take the process from one of the parents but choose 
**a random one** in the launchable processes or wait action.

### **Verification**

The verification program, krpsim_verif, **parse the input** file the same way as krpsim. It just **doesn't initialize**
the members used for **optimization**.

Then, it reads the trace file **line by line** (we only care about the lines shaped like `<cycle>:<process_name>`. 
Once the first one is found, must be continuous), checking that **each process launch is valid** (the process **exists**,
the **stocks needed are available** at the time of launch).
It **updates the stocks** according to the process needs (at launch) and results (when finished).

At the end of the trace, it **prints the final stocks and the last cycle**. If any **error** is found during 
the verification, it reports the error with **the cycle and process name**.