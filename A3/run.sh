#!/bin/bash


for algo in rand opt fifo lru clock; do 
	for trace in ./traceprogs/tr-blocked.ref ./traceprogs/tr-matmul.ref ./traceprogs/tr-simpleloop.ref; do
		for size in {50..200..50}; do
			echo "****************************************************"
			echo "Result for algo: " $algo 
			echo "trace: " $trace
			echo "size: " $size
			echo "****************************************************"
			echo " "
			./sim -f $trace -m $size -s 20000 -a $algo | tail --lines=7 > output.out
			echo " "
		done
	done
done