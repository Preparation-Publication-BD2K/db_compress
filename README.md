# SQUISH: Compression for Archival and Distribution of Structured Datasets

This repository contains code for near-optimal compression of relational datasets, using a combination of bayesian networks and arithmetic coding. 

Details are provided in the paper "Squish: Near-Optimal Compression for Archival of Relational Datasets", available at this link: http://arxiv.org/abs/1602.04256

The project is configured as a library, which can be used to create compression program for any relational file format.

An example program using SQUISH can be found in examples/sample.cpp, which compresses csv-style file:

'''
#Compress:
./sample -c covtype.data covtype.compressed covtype.config
#Decompress:
./sample -d covtype.compressed covtype.recovered covtype.config
'''

SQUISH allows user to define new data types and create associated SquID such that they can be compressed using SQUISH. The interface of SquID can be found in model.h. The SquIDModel class allows more flexible SquID creation. It is optional in the sense that most functions can simply return 0 or do nothing.

In SQUISH all primitive data types are implemented using SquID:
     categorical\_model.h/cpp
     numerical\_model.h/cpp
     string\_model.h/cpp
A simpler SquID example built upon numerical SquID can be found in examples/corel.h.
