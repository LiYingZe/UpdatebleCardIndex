# UpdatebleCardIndex

## Description：
The First Updatabe CardIndex, aims to solve CE tasks in dynamic enviroments.
Because of our lightweight network structure, it enables efficient incremental training.
We are currently implementing the combination of deep network and B+ tree.
The amortized insertion cost of 1 micros/tuple is realized.

## File organization structure
CardIndexExceute.cpp:  Create a cardIndex, use it to perform CE and Index tasks.

IncrementalTraining.py: Use Incremental Training to train our lightweight MADE network. 

loadDataSet.py: read data from numpy files. And transform them into Z-Ordered files.

prepare.bat: Batch file processing, performing data preparation, training network, etc.
