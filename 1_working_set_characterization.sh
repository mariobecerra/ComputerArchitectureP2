#!/bin/bash

trace_files=("spice" "tex" "cc")

mkdir stats_out

rm stats_out/spice_stats_1_wsc.txt
rm stats_out/tex_stats_1_wsc.txt
rm stats_out/cc_stats_1_wsc.txt

echo -e '\n\n'

k=0

for f in "${trace_files[@]}"
	do
	:
	for ((i=1; i <= 268435456 ; i=2*i));
		do
		k=$(($i*4))
		echo -e 'Executing ' $f 'with cache size ' $k
		./sim -is $k -ds $k -bs 4 -a $i -wb -wa trazas/$f.trace >> stats_out/$f'_stats_1_wsc.txt'
		echo -e '\n\n\n\n\n\n\n\n\n\n\n' >> stats_out/$f'_stats_1_wsc.txt'
	done
done









