# Concurrency analysis based on Ml training with run/compile time data 

The basic idea is to train a ML model, in an unsupervised self-testing environment, advanced patterns of concurrency issues. The goal is to have it output a probability value for any given piece of SICCL(see below) code without being trained on a dataset.

Note: Throwing Ml against NP complete problems is nothing new, in this case it's especially difficult because tripping a certain kind of concurrency issues can take a long time, which is suboptimal for Ml training.

## Abstract Flow of operations

The data on which the model gets trained is simple python code that is encoded as SICCL which is a graph that contains all information (mutex/ var/ thread ids) which contains all abstract relative information required to detect a possible race condition. The model's output is an action with which it can change mutex id's with the SICCL graph so that possible race conditions get fixed.
After every iteration, the SICCL graph is converted to python code and executed. The generated python code includes a loop for every thread, which increases every variable by one. That way, since array variables are not atomic, if the accumulated loop count of all threads in which a variable is present, does not equal its own value, we know that there is a possible concurrency issue.
That way of measuring works great and since the deviation isn't binary but very scalar it's a great learning basis for the model.

# Todo

- [x] 1. Approximating feasibility of the project
    - Creating an abstract formal language describing different concurrency configurations(from which a python program is generated) seems to be possible
    - Building a graph from compile and runtime data is possible and has been done by thread/ mem sanitizers (as real model input samples)
- [x] 2. Writing a simulation environment that generates a concurrent program from an SICCL like abstraction
- [x] 3. Putting it all together
- [] 4. Finding the necessary complexity of SICCL such that it is complex enough to resemble most code converted, but not too complex that it would overwhelm the model.
- [] 5. add runtime data as training input i.e. resolve and abstract crash data and stack traces.

## Log Book

### Disclaimer

I'm starting by testing whether it can solve concurrency issues that require only a simple linear heuristic: Every variable id gets an exclusive mutex id.

### First working version ([commit](https://github.com/luickk/MlBasedConcurrencyAnalysis/commit/51d400eff8cbf0ef557caf6cc3c442c4b9b7f9e8))

This commit contains the first working version of the project. The network manages to assign new variables the same mutexe over different threads. Although being a very(very) simple task to accomplish, the little time it took the network to get there makes me very optimistic that when scaling the complexity and context information, the network will be able to keep up.

Initial SICCL matrix:

```
# thread name, parent arr, var name, mutex name
siccl_example_flattened = np.array([[1, 0, 2, 0],
                                    [1, 0, 3, 0],
                                    [5, 1, 2, 0], 
                                    [5, 1, 3, 0], 
                                    [6, 5, 2, 0], 
                                    [6, 5, 3, 0], 
                                    [7, 5, 2, 0]], dtype=int)
```

After 10 iterations:

```
[[1 0 2 0]
 [1 0 3 9]
 [5 1 2 0]
 [5 1 3 1]
 [6 5 2 1]
 [6 5 3 1]
 [7 5 2 0]]
```
Missing non atomic operations:  
- 3: 1567951.0
- 3: 259231.5
- Reward: 0.695464946900354
- Loss: -3


After 200 iterations
```
[[1 0 2 0]
 [1 0 3 1]
 [5 1 2 0]
 [5 1 3 1]
 [6 5 2 1]
 [6 5 3 1]
 [7 5 2 0]]
2: 250748.0
3:  0.5
```
Missing non atomic operations:  
- 250748.0
- 0.5
- Reward: 0.9582021953186979
- Loss: 1


### New approach with DeepQ learning

#### Comparison between old and new Model

- DeepQ Model after 500 Model updates:

    ```[[1 0 2 0]
     [1 0 3 1]
     [5 1 2 0]
     [5 1 3 1]
     [6 5 2 0]
     [6 5 3 1]
     [7 5 2 2]]
     ```
     Absoulte atomic error rate: 1.5

 - Actor-Critic after 500 Model updates:
    ```
    [1 0 2 2
     1 0 3 2 
     5 1 2 2 
     5 1 3 2 
     6 5 2 1 
     6 5 3 2 
     7 5 2 1]
     ```
    Absoulte atomic error rate: 2.1666666665114462

Interestingly enough, the networks managed to reach lower absolute atomic error rates (they reached lower probabilities of a concurrency issue occurring), but not through a linear heuristic such as: every variable gets an exclusive mutex, but through more abstract means of "guessing". 
That's either, and actually very likely, induced through means of measurement (way of calculating the absolute atomic error rate) or actually a more safe configuration.

# Approach

## ML model

Since it's difficult to generate a dataset, a reinforcement learning approach is the most obvious to choose. The great problem is the generation of sample programs that are executable and *have a high enough entropy*.

Since generating python code is simply too complex for a simple reinforcement learning model, I created SICCL(Simple Initial Concurrency Configuration Language -> see below), which acts as an abstraction layer to the reinforcement learning model. Since finding a proper policy function for a "language" is difficult, I will be using a [model free approach](https://en.wikipedia.org/wiki/Model-free_(reinforcement_learning)).

The basic values the model will optimize for are, size (length of the SICCL script), and validity. The generated SICCL is then run and a score for missed non atomic operations is generated which directly correlates with how concurrency hardened the SICCL (and with that the program it was generated from) is.

## Alternatives to Reinforcement learning

[This](https://cs229.stanford.edu/proj2016/report/ChenYingLaird-DeepQLearningWithRecurrentNeuralNetwords-report.pdf) paper on patterns in long term data where the dependencies of the data nodes are far apart is really interesting. The problem with the reinforcement actor-critic model is that the data is really complex, with many data node dependencies/ patterns that may only reveal themselves after many iterations.

My first model, which actually worked ok (see results above) was a simple Actor-Ciritic Reinforcement Model which is not optimal for long term behaving complex systems.
That's why in the new iteration I'm trying the above mentioned DeepQ-Reinforced Model which is optimized for long term and more complex problems.

## Concurrency issue detection

Concurrency issue detection can be a very complex endeavor but since we don't need a high degree of abstraction or readability but really only some indication value we will keep it simple. In python, not all operations on lists are atomic so that they just miss in the end result. That is quite useful if you know what they should be. So the concurrency issue indicator will be a function of how many non atomic operations on a list have not been "executed" (e.g. are missing) compared to how many should have been "executed, the delta is the indicator.

The from the SICCL script generated python script includes the calculation of the indicator(the delta of all threads(loops) in which the variable is increased by one, and the actual value of the variable).
If there is not enough mutexe, the add operation might not be applied on the variable, which does not get increased as a result. The greater the difference of the value, the variable should have had(the sum of all the loops it's referenced in) and the actual value, the more collisions were happening.
That does not compensate for potential data races, but that can be added later.
SICCL generation goes both ways, so new SICCL scripts can be generated from existing python code (through ast analysis), and used as training data.

The biggest challange will be to make the SICCL script as detailed and simple as possible, to maintain all of the necessary concurrency complexity from the python code, and keep out the rest.

### Detection results

This very simple indicator works great with a very low error rate compared to its delta of mutex vs no mutex usage.

- Results with 2 variables, 4 threads, with 2(different) mutexe and 1 sec runtime:
    Script:
    ```python
    #   parent arr, thread id, var id, mutex id
    siccl_example_flattened = np.array([[1, 0, 2, 2],
                                        [1, 0, 3, 3],
                                        [5, 1, 2, 2], 
                                        [5, 1, 3, 3], 
                                        [6, 5, 2, 2], 
                                        [6, 5, 3, 3], 
                                        [7, 5, 2, 2]], dtype=int) 
    ```
    Results:
    ```
    var 2 diff:  -57
    var 3 diff:  -32
    ```
    A negative difference should theoretically not be possible, since it would indicate that there have been more iterations applied on a variable than there actually were. That's the error, which is somewhere in the lower second digits, which is negligible compared to the non-atomic addition operation misses if there are no mutexe.

- Results with 2 variables, 4 threads, *0* mutexe and 1 sec runtime:
    Script:
    ```python
    #   parent arr, thread id, var id, mutex id
    siccl_example_flattened = np.array([[1, 0, 2, 0],
                                        [1, 0, 3, 0],
                                        [5, 1, 2, 0], 
                                        [5, 1, 3, 0], 
                                        [6, 5, 2, 0], 
                                        [6, 5, 3, 0], 
                                        [7, 5, 2, 0]], dtype=int) 
    ```
    Results:
    ```
    var 2 diff:  3604555
    var 3 diff:  1838930
    ```
    As we can see, without any mutexe, the amount of missed non-atomic addition operation increases drastically per variable.

### Simple Initial Concurrency Configuration Language (SICCL)

SICCL is a very rudimentary formal language describing initial concurrent configurations of a program. 
The only entities are threads and variables. Every new dimension resembles a new thread. 
In the future the language could include grammar/ syntax for locking, syncing, timing delays to provide a reduced but complex enough environment for the model to experiment with.

Originally I wanted to go with a multidimensional tree like structure where every new axis was a new thread. That was a lot more elegant, not just for readability, but also for generation. The problem was that numpy/ tensorflow doesn't really support multi axis arrays with different sizes, and transforming (mostly padding) them is more complex than just going with an already flattened(static in size and axes) structure. The new flattened version has the problem that thread information needs to be encoded as well.
For example:
```python
# thread id, var id, mutex id
[0, 1, 2, 4],
[0, 1, 3, 4],
[1, 5, 2, 4], 
[1, 5, 3, 0], 
[5, 6, 2, 4], 
[5, 6, 2, 4], 
[5, 7, 2, 4]
```
compiles to:

```python
#!/usr/bin/python3

import threading
import time
from threading import Thread

exit_loops = False

def end_loops_timer_thread():
    time.sleep(5)
    globals()["exit_loops"] = True

def main():
    loop_stop_thread = Thread(target=end_loops_timer_thread, args=()) 
    loop_stop_thread.start()
    var_2 = [92, 87]
    var_3 = [28, 27]
    t_5 = Thread(target=thread_5, args=(var_2, var_3,)) 
    t_5.start()
    while not exit_loops:
        var_2[0] = 47
        var_3[0] = 88

def thread_5(var_2, var_3):
    t_6 = Thread(target=thread_6, args=(var_2,)) 
    t_6.start()
    t_7 = Thread(target=thread_7, args=(var_2,)) 
    t_7.start()
    while not exit_loops:
        var_2[0] = 45
        var_3[0] = 10

def thread_6(var_2):
    while not exit_loops:
        var_2[0] = 100
        var_2[0] = 12

def thread_7(var_2):
    while not exit_loops:
        var_2[0] = 87

if __name__ == "__main__":
    main()

```
## Fundamentals

It's difficult to find information on how modern thread sanitizers work and existing sanitizers are hidden in or built on top of big codebases which makes it difficult to learn from them. [This](https://static.googleusercontent.com/media/research.google.com/de//pubs/archive/35604.pdf) is a great paper on the fundamentals though.

## Tools for Instrumentation/ Interception, AST analysis

Since the approach of this project to generate a proper learning environment for the Ml model is a mix of run and compiletime data, both AST code and runtime information need to be combined into one graph.
For AST parsing and analysis, LLVM offers libclang which is really handy and covers all requirements. 

In order to generate a somewhat complete graph, we need the memory access information, which can only be read through memory instrumentation.
Intercepting function calls alone will not be enough but could have been easily achieved through [Clangs natively supported](https://maskray.me/blog/2023-01-08-all-about-sanitizer-interceptors) fn wrapper functionality. 

For memory instrumentation, there are multiple external tools available.
- QBDI is great but lacks support of multithreading(?)
- Frida is a really heavy framework that is not suited for fast program analysis evolutions as required for Ml training
- DynamoRIO requires a "runtime" or a binary that inserts the generated shared lib into the target process but supports multiple threads and enables somewhat(relatively speaking) lightweight memory instrumentation.

Since it took some time to find the right literature, here a few links:
- https://gcc.gnu.org/wiki/cauldron2012?action=AttachFile&do=get&target=kcc.pdf
- https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/data-race-test/ThreadSanitizerLLVM.pdf
- https://github.com/google/sanitizers/wiki/ThreadSanitizerAlgorithm
- https://storage.googleapis.com/pub-tools-public-publication-data/pdf/37278.pdf
- https://storage.googleapis.com/pub-tools-public-publication-data/pdf/35604.pdf
- https://github.com/google/sanitizers/wiki/ThreadSanitizerAlgorithm
- https://github.com/MattPD/cpplinks/blob/master/analysis.dynamic.md#software-sanitizers
- https://valgrind.org/docs/valgrind2007.pdf
- https://github.com/QBDI/QBDI

