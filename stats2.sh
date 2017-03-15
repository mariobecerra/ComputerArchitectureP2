#!/bin/bash

for ((i=1; i <= 64 ; i=2*i));
do
  ./sim -is 8192 -ds 8192 -bs 128 -a $i -wb -wa trazas/spice.trace >> spice_stats_assoc.txt
  ./sim -is 8192 -ds 8192 -bs 128 -a $i -wb -wa trazas/tex.trace >> tex_stats_assoc.txt
  ./sim -is 8192 -ds 8192 -bs 128 -a $i -wb -wa trazas/cc.trace >> cc_stats_assoc.txt
done









