#### PathExplorer: a Pintool for Binary Code Covering 

##### The current version is UNSTABLE, but the following are less unstable:

* Dynamic tainting: construct the dataflow graph based on the liveness analysis (using the outer interface of live variables).
* Checkpoint detection: for each conditional branch there are several execution points which may affect to its decision.
* Reverse execution: an application-layer reverse execution mechanism.

##### In development:
* Smarter treatment for multiple rollbacks in case of direction fields in the input.
* DFA approximation for CFG.


##### Known bugs:

* Does not work for multiple threads programs yet.
* Re-execution is lost for large CFGs after too much rollbacks (lost detected in testing for a CFG with depth of 1000 instructions after nearly 150.000.000 rollbacks).