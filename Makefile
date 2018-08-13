UV_DIR ?= uv
UV_BUILD = $(UV_DIR)/out/$(BUILDTYPE)

BUILDTYPE ?= Release
IDIR = $(UV_DIR)/include

webserver: webserver.c libuv.a http_parser/http_parser.o
	gcc -o webserver webserver.c \
		$(UV_BUILD)/libuv.a \
		http_parser/http_parser.o -I$(IDIR)

libuv.a: 
	@cd $(UV_DIR); \
    ./gyp_uv.py -f make > /dev/null; \
    BUILDTYPE=$(BUILDTYPE) make -C out

http_parser/http_parser.o:
	$(MAKE) -C http_parser http_parser.o
