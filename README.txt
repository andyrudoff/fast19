#
# FAST 2019 Tutorial on Advanced Persistent Memory Programming
# February 25, 2019
# Boston
#
# This repo contains the code examples used during the tutorial,
# fully buildable.  In addition, this README outlines the sys admin
# commands used during the demo portion of the tutorial, as well
# as some other useful commands (cloning PMDK, building it, etc).
#
# Many of the sys admin steps described here can be found in the
# Getting Started Guide on pmem.io:
https://docs.pmem.io/getting-started-guide

#
# To clone this repo, use something like the command below.
# (Examples assume cloning in your home directory, hence the "cd")
#
cd
git clone https://github.com/andyrudoff/fast19

#
# Checking to make sure your kernel supports pmem:
#
uname -r	# see kernel currently running
grep -i pmem /boot/config-`uname -r`
grep -i nvdimm /boot/config-`uname -r`

#
# Use ndctl to show that you have pmem installed:
#
ndctl list -u

#
# Intel-specific:
#
ipmctl show -dimm
ipmctl show -memoryresources

#
# Create a DAX-capable file system and mount it:
#
mkdir /mnt/pmem-fsdax0 /mnt/pmem-fsdax1
mkfs.ext4 /dev/pmem0
mkfs.ext4 /dev/pmem1
mount -o dax /dev/pmem0 /mnt/pmem-fsdax0
mount -o dax /dev/pmem1 /mnt/pmem-fsdax1
df -h

#
# Some of the examples use PMDK.  Many distros include PMDK, but
# it takes some time for the latest version to flow intro the distros
# so here are the steps to build the latest source and install it in
# the usual locations.
#
cd
# ubuntu: sudo apt-get install autoconf pkg-config libndctl-dev libdaxctl-dev
sudo dnf install autoconf pkg-config ndctl-devel daxctl-devel
git clone https://github.com/pmem/pmdk
cd pmdk
make
sudo make install prefix=/usr

#
# Also install the C++ bindings for libpmemobj for the C++ example.
#
cd
# ubuntu: sudo apt-get install cmake
sudo dnf install cmake
git clone https://github.com/pmem/libpmemobj-cpp
cd libpmemobj-cpp
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
make
sudo make install

#
# Now everything in the fast19 repo should build...
#
cd
cd fast19
make

#
# raw.c contains the simplest possible memory-mapped pmem example
# this illustrates how to use mmap().  nothing transactional here!
#
dd if=/dev/zero of=/mnt/pmem-fsdax0/daxfile bs=4k count=1
od -c /mnt/pmem-fsdax0/daxfile
./raw /mnt/pmem-fsdax0/daxfile
od -c /mnt/pmem-fsdax0/daxfile

#
# freq.c
#
# Simple C program for counting word frequency on a list on text files.
# It uses a hash table with linked lists in each bucket.
#
# This just sets the stage with a simple programming example.  Nothing
# related to pmem yet.
#
./freq -p words.txt

#
# freq_mt.c
#
# Convert freq.c to be multi-threaded.  A separate thread is created
# for each text file.
#
./freq_mt -p words.txt words.txt words.txt

#
# freq_pmem.c
#
# Convert freq_mt.c to mode the hash table to persistent memory and use
# libpmemobj for pmem allocation, locking, and transactions.
#
# We also use freq_pmem_print.c to display the currrent hash table contents.
#
pmempool create obj --layout=freq -s 100G /mnt/pmem-fsdax0/freqcount
pmempool info /mnt/pmem-fsdax0/freqcount
./freq_pmem_print /mnt/pmem-fsdax0/freqcount
./freq_pmem /mnt/pmem-fsdax0/freqcount words.txt words.txt words.txt
./freq_pmem_print /mnt/pmem-fsdax0/freqcount

#
# freq_pmem_cpp.c
#
# Similar to freq_pmem.c but uses the C++ bindings to libpmemobj.
#
./freq_pmem_cpp /mnt/pmem-fsdax0/freqcount words.txt words.txt words.txt

#
# You brought in many more examples when you cloned the PMDK tree:
#
cd
cd pmdk/src/examples
