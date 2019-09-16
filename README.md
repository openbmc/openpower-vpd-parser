# keyword-vpd-parser
Keyword VPD parser provides mechanism to parse the 
binary keyword VPD file and store the keyword-value pairs
inside the map.

## To Build
```
Config commands
	1. export LC_ALL="C"
	2. autoreconf -if
	3. ./bootstrap.sh clean 
	4. ./bootstrap.sh
	5. ./configure ${CONFIGURE_FLAGS} --prefix=/usr --enable-kw-vpd-parser

Generate binary keyword VPD parser
	1. make clean
	2. make
```
