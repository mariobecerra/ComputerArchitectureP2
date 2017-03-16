#!/bin/bash

# grep '(demand fetch:[\t0-9\.]+)|(copies back:[\t0-9\.]+)' -P  stats_out/tex_stats_4_memory_bandwidth.txt


trace_files=("spice" "tex" "cc")
wa_nw=("-wa" "-nw")
wt_wb=("-wt" "-wb")

mkdir stats_out

rm stats_out/spice_stats_4_memory_bandwidth.txt
rm stats_out/tex_stats_4_memory_bandwidth.txt
rm stats_out/cc_stats_4_memory_bandwidth.txt

echo -e '\n\n'

for f in "${trace_files[@]}"
	do
	for ((cs = 8192; cs <= 16384 ; cs = 2*cs)); # cache size
		do
		for ((bs = 64; bs <= 128 ; bs = 2*bs)); # block size
			do
			for ((as = 2; as <= 4 ; as = 2*as)); # associativity
				do
				for an in "${wa_nw[@]}" # write policy when miss
					do
					for bt in "${wt_wb[@]}" # write policy when hit
						do
						echo -e 'Executing' $f 'with\n\tassociativity' $as '\n\tcache size' $cs '\n\tblock size' $bs '\n\twrite pollicies' $an $bt
						./sim -is $cs -ds $cs -bs $bs -a $as $bt $an trazas/$f.trace >> stats_out/$f'_stats_4_memory_bandwidth.txt'
						echo -e '\n\n\n\n\n\n\n\n\n\n\n' >> stats_out/$f'_stats_4_memory_bandwidth.txt'
					done
				done
			done
		done
	done
done


