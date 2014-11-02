#### PathExplorer: extracts high-order properties from Binary Codes

##### Obsolete information (update later): the current version is UNSTABLE, but the following are less unstable :

* Dynamic tainting: constructs the dataflow graph based on the liveness analysis (using the outer interface of live variables).
* Checkpoint detection: for each conditional branch there are several execution points which may affect to its decision.
* Reverse execution: an application-layer reverse execution mechanism.
* Multiple rollbacks detection: handles direction fields in the input.
* Execution with jumps: a new algorithm for checkpoint detection to shorten re-execution paths.
* LTS approximation for the execution tree.

##### In development:

* IR lifting with BAP.

##### Known bugs:

* Does not work for multiple threads programs yet.
* (Not a bug but) because of heavily using several C++11 features (smart pointer, lambda, type deduction, etc) the code cannot be compiled in old C++ compilers (e.g. VS(i) where i < 10).

