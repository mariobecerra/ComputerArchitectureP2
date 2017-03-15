#!/bin/bash

# trace_files=($(ls trazas))
# declare -a ass=("2" "4" "8" "16" "32" "64")

# for i in "${trace_files[@]}"
# do
#    :
#    for a in "${ass[@]}"
#    do
#    :
#    		echo -e '\n\n\n'$i
#    		./sim -ds 8192 -is 8192 -bs 128 -a $a trazas/$i
#    	done
# done

trace_files=("spice" "tex" "cc")

mkdir stats_out

rm stats_out/spice_stats_3_assoc.txt
rm stats_out/tex_stats_3_assoc.txt
rm stats_out/cc_stats_3_assoc.txt

echo -e '\n\n'

for f in "${trace_files[@]}"
	do
	:
	for ((i=1; i <= 64 ; i=2*i));
		do
		echo -e 'Executing ' $f 'with associativity ' $i
		./sim -is 8192 -ds 8192 -bs 128 -a $i -wb -wa trazas/$f.trace >> stats_out/$f'_stats_3_assoc.txt'
		echo -e '\n\n\n\n\n\n\n\n\n\n\n' >> stats_out/$f'_stats_3_assoc.txt'
	  
		# ./sim -is 8192 -ds 8192 -bs 128 -a $i -wb -wa trazas/tex.trace >> stats_out/tex_stats_3_assoc.txt
		# echo -e '\n\n' >> stats_out/tex_stats_3_assoc.txt

		# ./sim -is 8192 -ds 8192 -bs 128 -a $i -wb -wa trazas/cc.trace >> stats_out/cc_stats_3_assoc.txt
		# echo -e '\n\n' >> stats_out/cc_stats_3_assoc.txt
	done
done


