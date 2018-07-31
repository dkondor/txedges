# extract one day's data from the Bitcoin dataset
# requires:
# 	fish shell -- https://fishshell.com/
# 	(uses fish's syntax for shell variables and command substitution -- should be straightforward to port to bash)
# numjoin from https://github.com/dkondor/join-utils (for joining text files sorted by a numeric field)
# transaction edge generating program from https://github.com/dkondor/txedges

# preparation: download and compile the numeric join utility needed
git clone https://github.com/dkondor/join-utils.git
cd join-utils
g++ -o numjoin numeric_join.cpp -std=gnu++11 -O3 -march=native
cd ..
# download and compile the transaction matching program
# git clone https://github.com/dkondor/txedges.git
# cd txedges
# g++ -o txedge txedge.cpp -std=gnu++14 -O3 -march=native
# cd ..

# 1. set which day we're interested in
set day 2018-02-07 # or day=2018-02-07 in bash
set ts_start (date -d $day -u +%s) # or ts_start=`date -d $day -u +%s` in bash
echo $ts_start # 1517961600


# 2. select blockIDs happening in that day
zcat bh.dat.gz | awk "{if(\$3 >= $ts_start && \$3 < $ts_start + 86400) print \$1,\$3;}" > blockids_$day.dat
# 141 blocks in total


# 3. select txIDs happening in that day
xzcat tx.dat.xz | ./join-utils/numjoin -1 2 -2 1 -o1 1 -o2 2 - blockids_$day.dat > txids_$day.dat
# Matched lines from file 1: 213719
# Matched lines from file 2: 141
# Total lines output: 213719


# 4. filter transaction inputs and outputs for this day
xzcat txin.dat.xz | ./join-utils/numjoin -o1 "" txids_$day.dat - > txin_$day.dat
# Matched lines from file 1: 213578 -- note: mining transactions have no inputs
# Matched lines from file 2: 756083
# Total lines output: 756083

xzcat txout.dat.xz | ./join-utils/numjoin -o1 "" txids_$day.dat - > txout_$day.dat
# Matched lines from file 1: 213719
# Matched lines from file 2: 588509
# Total lines output: 588509


# 5. use the specialized join program to distribute transfered money values
./txedges/txedge -i txin_$day.dat -o txout_$day.dat > txedges_$day.dat
# 213578 transactions matched, 2163171 edges generated


# 6. join with transaction timestamps
./join-utils/numjoin -o2 2,3,4 txids_$day.dat txedges_$day.dat > txedges_ts_$day.dat
# Matched lines from file 1: 213578
# Matched lines from file 2: 2163171
# Total lines output: 2163171



# 7. get the mining transaction outputs, join these with transaction timestamps as well
./join-utils/numjoin -v 2 -o1 "" -o2 1,3,4 txin_$day.dat txout_$day.dat > tx_mining_$day.dat
# Matched lines from file 1: 0
# Matched lines from file 2: 0
# Unmatched lines from file 2: 294
# Total lines output: 294
./join-utils/numjoin -o2 2,3 txids_$day.dat tx_mining_$day.dat > tx_mining_ts_$day.dat
# Matched lines from file 1: 141
# Matched lines from file 2: 294
# Total lines output: 294



