#!/bin/bash

trace_files=("spice" "tex" "cc")

mkdir stats_out

rm stats_out/spice_stats_2_block.txt
rm stats_out/tex_stats_2_block.txt
rm stats_out/cc_stats_2_block.txt

echo -e '\n\n'

for f in "${trace_files[@]}"
	do
	:
	for ((i=4; i <= 4096 ; i=2*i));
		do
		echo -e 'Executing ' $f 'with block size ' $i
		#./sim -is 8192 -ds 8192 -bs 128 -a $i -wb -wa trazas/$f.trace >> stats_out/$f'_stats_3_assoc.txt'
		./sim -is 8192 -ds 8192 -bs $i -a 2 -wb -wa trazas/$f.trace >> stats_out/$f'_stats_2_block.txt'
		echo -e '\n\n\n\n\n\n\n\n\n\n\n' >> stats_out/$f'_stats_2_block.txt'
	done
done









