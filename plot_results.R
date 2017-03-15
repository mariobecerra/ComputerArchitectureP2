library(tidyverse)
library(utilsMBC)

number_ticks <- function(n) {function(limits) pretty(limits, n)}
theme_set(theme_bw(base_size = 13))

wsc <- read_csv("result_files/1_wsc.csv")
block_size <- read_csv("result_files/2_block_size.csv")
assoc_size <- read_csv("result_files/3_associativity_size.csv")

(wsc %>% 
    ggplot(aes(Power, Hit_Rate)) +
    geom_point(size = 0.8) +
    geom_line(aes(linetype = Type), size = 0.6) +
    facet_grid(~File) +
    xlab("Tamaño de caché (2^x)") +
    ylab("Hit Rate") +
    scale_x_continuous(trans='log2') 
) %>% 
  ggsave(., 
         file = "result_files/1_wsc.pdf",
         device = "pdf")

(block_size %>% 
    ggplot(aes(Block_size, Hit_Rate)) +
    geom_point(size = 0.8) +
    geom_line(aes(linetype = Type), size = 0.6) +
    facet_grid(~File) +
    xlab("Tamaño de bloque") +
    ylab("Hit Rate") +
    scale_x_continuous(trans='log2')
) %>% 
  ggsave(., 
         file = "result_files/2_block_size.pdf",
         device = "pdf")


(assoc_size %>% 
    ggplot(aes(Associativity, Hit_rate)) +
    geom_point(size = 0.8) +
    geom_line(aes(linetype = Type), size = 0.6) +
    facet_grid(~File) +
    xlab("Asociatividad") +
    ylab("Hit Rate") +
    scale_x_continuous(trans='log2') 
) %>% 
  ggsave(., 
         file = "result_files/3_assoc_size.pdf",
         device = "pdf")

