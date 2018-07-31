# txedges
Join Bitcoin transaction inputs and outputs to create an approximate weighted graph between addresses.

This program converts the list of transaction inputs and outputs to a list of directed weighted edges between addresses. Input is expected in the format similar to the files available at https://senseable2015-6.mit.edu/bitcoin/. Notably, transaction IDs and address IDs are expected to be nonnegative integers with the special address value -1 also accepted (this denotes addresses that could not be decoded in the aforementioned dataset). No special handling is performed for this special value, i.e. all appearances of address -1 are treated as it was a normal address. Currently, transaction IDs are expected to be less than 2^32, while address IDs are expected to be less than 2^31. The input files are expected to be sorted by transaction IDs.

Output is written to the standard output as TSV with columns: txID, in\_addr, out\_addr, weight

A transaction with inputs from N distinct addresses and outputs to M distinct addresses is processed to NxM directed edges, i.e. each input address is connected to all output addresses. The edges are weighted according to the sums transferred, as if each input address separately divided its contribution (minus the proportional share of transaction fees) among all output addresses according to their shared in the output. E.g. if a transaction has two input addresses, A and B contributing 13 and 26 BTC respectively and output addresses C, D and E receiving 6, 12 and 18 BTC respectively (thus having 3 BTC transaction fees in total), this program will generate the following edges with the corresponding weights:

A->C 2
A->D 4
A->E 6
B->C 4
B->D 8
B->E 12

Note that the output does not include any information on transaction fees (and also, that the values given in this example are quite unrealistic). All input and output sums are written in Satoshis. Note that due to the way edge weights are assigned with simply dividing the input amounts with the corresponding weights, the output can include fractional Satoshi values and will potentially introduce rounding errors. No attempt is made to correct for these (a more sophisticated approach could try to distribute edge weights in a way that all are preserved as whole numbers).

The output also does not include mining transactions (transactions with zero inputs); these should be processed separately if needed.

## Example usage

Compilation requires C++14 support, e.g. with gcc:

g++ -o txedge txedge.cpp -std=gnu++14 -O3 -march=native

Example run for the whole dataset:

./txedge -ix txin.dat.xz -ox txout.dat.xz > txedges.dat

Command line arguments specify the file containing transaction inputs (-i) and transaction outputs (-o). Appending a 'z' to either means that the file is compressed with gzip, appending an 'x' means that the file is compressed with xz (as in the above example).

Further example use to extract transactions only for one day is given in the txedge_1day.sh script.



