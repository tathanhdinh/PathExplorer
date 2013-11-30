#### PathExplorer: a Pintool for Binary Code Coveraging 

##### The current version is UNSTABLE, but the following are (hopefully) less unstable:

* Dynamic tainting: construct the dependant graph based on the liveness analysis (using the outer interface of live variables).
* Checkpoint detection: for each conditional branch there are several execution points which may affect to its decision.

##### In development:

* Reverse execution: an application-layer reverse execution mechanism.

##### Known bugs:

* Does not work for multiple threads programs.
* Re-execution is lost for large CFG after many rollbacks (lost detected in testing for a CFG with depth of 1000 instructions after nearly 2.000.000 rollbacks).