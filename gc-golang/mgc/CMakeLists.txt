enable_language(ASM)

aux_source_directory(. srcs)

add_library(mgc ${srcs} root.s)
