CC = gcc
DEBUG=-g
INCLUDE = -I.. -I../../common -I../../common/src
CFLAGS  = -W -Wall -Wbad-function-cast -Wmissing-prototypes -Werror -g -fsigned-char -DSASR -DFAST -DUSE_FLOAT_MUL -DDEBUG_MEM
LIBS = -L../../common/src -lm -luclmmbase 

CHANNEL_OBJS = ../cc_vanilla.o ../new_channel.o ../channel_types.o

G728_LIB = -L../../g728lib -lg728
CODEC_LIB = -L.. -luclcodec 
CODEC_OBJS=$(CODEC_LIB)
SNDFILE_LIB = -L../ -luclsndfile

REPAIR_OBJS  = ../repair.o ../audio_util.o

UTIL_OBJS = ../timers.o ../playout.o ../ts.o

3D_OBJS = test_3d.o ../render_3D.o

RDNCY_OBJS = ../cc_rdncy.o ../channel_types.o test_rdncy.o

PKTBUF_OBJS = ../pktbuf.o test_pktbuf.o ../ts.o ../playout.o

RTONE_OBJS = ./rtone.o

all: test_convert rtone test_pktbuf test_repair 

#test_sndfile: $(CODEC_OBJS) test_sndfile.o
#	$(CC) $(DEBUG)  -o $@ $(SNDFILE_LIB) $(CODEC_LIB) $(LIBS) test_sndfile.o

test_convert: test_convert.o
	$(CC) $(DEBUG)  -o $@ test_convert.o $(CODEC_OBJS) $(LIBS) 

rtone: $(RTONE_OBJS) $(SNDFILE_OBJS)
	$(CC) $(DEBUG) -o $@ $(RTONE_OBJS) $(SNDFILE_LIB) $(CODEC_LIB) $(LIBS)

test_pktbuf: $(PKTBUF_OBJS)
	$(CC) $(DEBUG)  -o $@ $(PKTBUF_OBJS) $(LIBS) 

test_repair: $(REPAIR_OBJS) test_repair.o 
	$(CC) $(DEBUG)  -o $@ test_repair.o $(SNDFILE_LIB) $(REPAIR_OBJS) $(CODEC_LIB) $(LIBS) 

test_rdncy: $(RDNCY_OBJS) $(UTIL_OBJS)
	$(CC) $(DEBUG) -o $@ $(RDNCY_OBJS) $(UTIL_OBJS) $(CODEC_LIB) $(LIBS)

test_3d: $(3D_OBJS) 
	$(CC) $(DEBUG)  -o $@  $(3D_OBJS) $(CODEC_LIB) $(LIBS)

native: native.o 
	$(CC) $(DEBUG)  -o $@ native.o $(CODEC_LIB) $(LIBS)

test_channel: $(CHANNEL_OBJS) $(UTIL_OBJS) test_channel.o
	$(CC) $(DEBUG)  -o $@  test_channel.o $(CHANNEL_OBJS) $(UTIL_OBJS) \
	$(CODEC_LIB) $(LIBS)
test_codecs: test_codecs.o
	$(CC) $(DEBUG)  -o $@ test_codecs.o $(CODEC_LIB) $(LIBS)

test_ts: $(UTIL_OBJS) test_ts.o
	$(CC) $(DEBUG)  -o $@ $(UTIL_OBJS) $(LIBS) test_ts.o

.c.o:
	$(CC) $(INCLUDE) $(CFLAGS) -c $< 
#	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(CODEC_OBJS) $(3D_OBJS) $(RDNCY_OBJS) $(UTIL_OBJS) test_3d \
		test_codecs native test_channel *.o


