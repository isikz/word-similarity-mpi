To compile and execute the program, MPI environment should be installed. The program could be compiled by the following command:

`mpicc <project_name>.c −o <object_file_name>.o` 

and can be executed by the following command:
`mpirun −np <number_of_processors> --oversubscribe ./<object_file_name>.o`

* `<number_of_processors>` should be a positive integer above 2.
* Number of words should be divided completely into (`<number_of_processors> -1`). 
* `<object_file_name>` is the runnable file.
* `-- oversubscribe` is for higher number of processors (11, 21 etc)
