#### PathExplorer: a Pintool for Binary Code Coveraging 

##### The current version is CRAPPY, but the following are (hopefully) stable:

* Dynamic tainting: construct the dependant graph based on the liveness analysis (using the outer interface of live variables).
* Checkpoint detection: for each conditional branch there are several execution points which may affect to its decision.

##### In development:

* Reverse execution: an application-layer reverse execution mechanism.