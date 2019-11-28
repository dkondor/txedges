####################################################################
# script to process Bitcoin data to create transaction graph

# new dataset up to 2018 february available here:
# https://senseable2015-6.mit.edu/bitcoin/

# further components:
# https://github.com/dkondor/join-utils
#	C# and c++ command line utility to join text files based on a numeric field
#	(for C#, compile with Mono; mkbundle can be used to create a version which
#	can be run without a Mono runtime)


# 1. create network edges from the transaction data file
git clone https://github.com/dkondor/join-utils.git
cd join-utils
g++ -o numjoin numeric_join.cpp -O3 -march=native -std=gnu++11
cd ..
mkfifo txin.fifo
mkfifo txout.fifo
xzcat txin.dat.xz >> txin.fifo &
xzcat txout.dat.xz >> txout.fifo &
./join-utils/numjoin -j 1 -o1 1,5 -o2 3 txin.fifo txout.fifo > edges.dat
# Matched lines from file 1: 746945793
# Matched lines from file 2: 809109697
# Total lines output: 2963246654
rm txin.fifo
rm txout.fifo

# 1.1. create unique edges from the previous
awk 'BEGIN{OFS="\t";}{if($2!=$3) print $2,$3;}' edges.dat | sort -k 1n,1 -k 2n,2 -S 64G | uniq > edges_uniq.dat

# 1.2. add timestamps to the list of all edges
gunzip bh.dat.gz
xzcat tx.dat.xz | ./numeric_join -1 2 -o1 1 -2 1 -o2 3 - bh.dat > txtime.dat
# Matched lines from file 1: 298325122
# Matched lines from file 2: 508241
# Total lines output: 298325122
./numeric_join -j 1 -o1 2,3 -o2 2 edges.dat txtime.dat > edges_ts.dat
# Matched lines from file 1: 2963246654                                            
# Matched lines from file 2: 297816881
# Total lines output: 2963246654





