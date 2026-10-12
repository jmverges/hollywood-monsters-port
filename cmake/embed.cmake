# cmake -DIN=<file> -DOUT=<file.c> -DNAME=<symbol> -P embed.cmake: the file's bytes as a C array <NAME>
# (with a terminating zero) and its length <NAME>_size.
file(READ ${IN} hex HEX)
string(LENGTH "${hex}" n)
math(EXPR size "${n} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" bytes "${bytes}")
file(WRITE ${OUT} "/* Generated from ${IN} by cmake/embed.cmake. */\nconst unsigned char ${NAME}[] = {\n${bytes}0 };\nconst unsigned ${NAME}_size = ${size};\n")
