Here's how to do Baum-Welch training with `cdec`.

## Set the tags you want.

First, set the number of tags you want in tagset.txt (these
can be any symbols, listed one after another, separated
by whitespace), e.g.:

    C1 C2 C3 C4

## Extract the parameter feature names

    ../mpi_extract_features -c cdec.ini -t train.txt

If you have compiled with MPI, you can use `mpirun`:

    mpirun -np 8 ../mpi_extract_features -c cdec.ini -t train.txt

## Randomly initialize the weights file

    sort -u features.* | ./random_init.pl > weights.init

## Run training

    ../mpi_baum_welch -c cdec.ini -t train.txt -w weights.init -n 50

Again, if you have compiled with MPI, you can use `mpirun`:

    mpirun -np 8 ../mpi_baum_welch -c cdec.ini -t train.txt -w weights.init -n 50

The `-n` flag indicates how many iterations to run for.

