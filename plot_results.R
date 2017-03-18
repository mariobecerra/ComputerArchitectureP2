library(tidyverse)
library(utilsMBC)
library(gridExtra)

number_ticks <- function(n) {function(limits) pretty(limits, n)}
theme_set(theme_bw(base_size = 13))

wsc <- read_csv("result_files/1_wsc.csv")
block_size <- read_csv("result_files/2_block_size.csv")
assoc_size <- read_csv("result_files/3_associativity_size.csv")
memory_bandwidth <- read_csv("result_files/4_memory_bandwidth.csv")
trace_cc <- read_delim("trazas/cc.trace",
                       delim = ' ',
                       col_names = c("Inst", "Loc"))

trace_spice <- read_delim("trazas/spice.trace",
                       delim = ' ',
                       col_names = c("Inst", "Loc"))

trace_tex <- read_delim("trazas/tex.trace",
                       delim = ' ',
                       col_names = c("Inst", "Loc"))


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

# (memory_bandwidth %>% 
#     ggplot(aes(Associativity, Hit_rate)) +
#     geom_point(size = 0.8) +
#     geom_line(aes(linetype = Type), size = 0.6) +
#     facet_grid(~File) +
#     xlab("Asociatividad") +
#     ylab("Hit Rate") +
#     scale_x_continuous(trans='log2') 
# ) %>% 
#   ggsave(., 
#          file = "result_files/3_assoc_size.pdf",
#          device = "pdf")

# gg_cc_1 <- memory_bandwidth %>% 
#   filter(File == 'cc',
#          Block_Size == 64,
#          Cache_Size == "8K") %>% 
#   ggplot() + 
#   geom_bar(aes(factor(Associativity), Value, fill = WA_NW), 
#            stat = 'identity', 
#            position = 'dodge') + 
#   facet_grid(Type~WT_WB,
#              scales = 'free') + 
#   scale_fill_manual(values=c("black", "dark grey", "grey"))
# 
# gg_cc_2 <- memory_bandwidth %>% 
#   filter(File == 'cc',
#          Block_Size == 128,
#          Cache_Size == "8K") %>% 
#   ggplot() + 
#   geom_bar(aes(factor(Associativity), Value, fill = WA_NW), 
#            stat = 'identity', 
#            position = 'dodge') + 
#   facet_grid(Type~WT_WB,
#              scales = 'free') + 
#   scale_fill_manual(values=c("black", "dark grey", "grey"))
# 
# 
# gg_cc_3 <- memory_bandwidth %>% 
#   filter(File == 'cc',
#          Block_Size == 64,
#          Cache_Size == "16K") %>% 
#   ggplot() + 
#   geom_bar(aes(factor(Associativity), Value, fill = WA_NW), 
#            stat = 'identity', 
#            position = 'dodge') + 
#   facet_grid(Type~WT_WB,
#              scales = 'free') + 
#   scale_fill_manual(values=c("black", "dark grey", "grey"))
# 
# gg_cc_4 <- memory_bandwidth %>% 
#   filter(File == 'cc',
#          Block_Size == 128,
#          Cache_Size == "16K") %>% 
#   ggplot() + 
#   geom_bar(aes(factor(Associativity), Value, fill = WA_NW), 
#            stat = 'identity', 
#            position = 'dodge') + 
#   facet_grid(Type~WT_WB,
#              scales = 'free') + 
#   scale_fill_manual(values=c("black", "dark grey", "grey"))
# 
# (grid.arrange(gg_cc_1,
#              gg_cc_2,
#              gg_cc_3,
#              gg_cc_4,
#              ncol = 2)) %>% 
#   ggsave(., 
#          file = "result_files/4_wsc_cc.pdf",
#          device = "pdf")



#theme_set(theme_bw(base_size = 9))

plot_ind_wna_wa <- function(block_size, cache_size, file_name, df){
  df %>% 
    filter(File == file_name,
           Block_Size == block_size,
           Cache_Size == cache_size) %>% 
    ggplot() + 
    geom_bar(aes(factor(Associativity), Value, fill = WA_NW), 
             stat = 'identity', 
             position = 'dodge') + 
    facet_grid(Type~WT_WB,
               scales = 'free') + 
    scale_fill_manual(values=c("black", "dark grey", "grey")) +
    ggtitle(paste0("Tamaño de bloque: ", block_size, "\nTamaño de caché: ", cache_size)) +
    xlab("Asociatividad") +
    ylab("") +
    guides(fill=guide_legend(title=NULL)) +
    theme_bw(base_size = 6)
}



plot_ind_wt_wb <- function(block_size, cache_size, file_name, df){
  df %>% 
    filter(File == file_name,
           Block_Size == block_size,
           Cache_Size == cache_size) %>% 
    ggplot() + 
    geom_bar(aes(factor(Associativity), Value, fill = WT_WB), 
             stat = 'identity', 
             position = 'dodge') + 
    facet_grid(Type~WA_NW,
               scales = 'free') + 
    scale_fill_manual(values=c("black", "dark grey", "grey")) +
    ggtitle(paste0("Tamaño de bloque: ", block_size, "\nTamaño de caché: ", cache_size)) +
    xlab("Asociatividad") +
    ylab("") +
    guides(fill=guide_legend(title=NULL)) +
    theme_bw(base_size = 6)
}



print_4_wna_wa <- function(file_name, save = T){
  gg_1 <- plot_ind_wna_wa(64, "8K", file_name, memory_bandwidth)
  
  gg_2 <- plot_ind_wna_wa(128, "8K", file_name, memory_bandwidth)
  
  gg_3 <- plot_ind_wna_wa(64, "16K", file_name, memory_bandwidth)
  
  gg_4 <- plot_ind_wna_wa(128, "16K", file_name, memory_bandwidth)
  
  if(save){
    (grid.arrange(gg_1,
                  gg_2,
                  gg_3,
                  gg_4,
                  ncol = 2)) %>% 
      ggsave(., 
             file = paste0("result_files/4_memory_bandwidth_wna_wa_", file_name, ".pdf"),
             device = "pdf")
  } else {
    (grid.arrange(gg_1,
                  gg_2,
                  gg_3,
                  gg_4,
                  ncol = 2))
  }
  
}




print_4_wt_wb <- function(file_name){
  gg_1 <- plot_ind_wt_wb(64, "8K", file_name, memory_bandwidth)
  
  gg_2 <- plot_ind_wt_wb(128, "8K", file_name, memory_bandwidth)
  
  gg_3 <- plot_ind_wt_wb(64, "16K", file_name, memory_bandwidth)
  
  gg_4 <- plot_ind_wt_wb(128, "16K", file_name, memory_bandwidth)
  
  (grid.arrange(gg_1,
                gg_2,
                gg_3,
                gg_4,
                ncol = 2)) %>% 
    ggsave(., 
           file = paste0("result_files/4_memory_bandwidth_wt_wb_", file_name, ".pdf"),
           device = "pdf")
}



print_4_wna_wa("cc")
print_4_wna_wa("spice")
print_4_wna_wa("tex")

print_4_wt_wb("cc")
print_4_wt_wb("spice")
print_4_wt_wb("tex")




##############################
# Working set size
##############################

obtiene_wss <- function(df, tipo = 'datos'){
  if(tipo == 'datos') vec_tipo <- c(0, 1)
  if(tipo == 'instrucciones') vec_tipo <- 2
  
  # Número de localidades de memoria no únicas usadas en datos
  no_unicos <- df %>% 
    filter(Inst %in% vec_tipo) %>% 
    nrow()
  
  # Número de localidades de memoria únicas usadas en datos
  unicos <- df %>% 
    filter(Inst %in% vec_tipo) %>% 
    .$Loc %>% 
    unique() %>% 
    length()
  
  # # en bytes
  # unicos <- (unicos*32)/8
  # no_unicos <- (no_unicos*32)/8
  
  suma <- unicos + no_unicos
  
  unicos_porc = 100*unicos/suma
  no_unicos_porc = 100 - unicos_porc
  
  return(list(unicos = unicos, 
              no_unicos = no_unicos,
              unicos_porc = unicos_porc,
              no_unicos_porc = no_unicos_porc))
}

obtiene_wss(trace_cc, 'datos')
obtiene_wss(trace_cc, 'instrucciones')

obtiene_wss(trace_spice, 'datos')
obtiene_wss(trace_spice, 'instrucciones')

obtiene_wss(trace_tex, 'datos')
obtiene_wss(trace_tex, 'instrucciones')

