#!/bin/bash

for ((i=4; i <= 4096 ; i=2*i));
do
  ./sim -is 8192 -ds 8192 -bs $i -a 2 -wb -wa trazas/spice.trace >> spice_stats_block.txt
  ./sim -is 8192 -ds 8192 -bs $i -a 2 -wb -wa trazas/tex.trace >> tex_stats_block.txt
  ./sim -is 8192 -ds 8192 -bs $i -a 2 -wb -wa trazas/cc.trace >> cc_stats_block.txt
done









