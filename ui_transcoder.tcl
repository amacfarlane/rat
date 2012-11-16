#
# ui_transcoder.tcl
#
# Copyright (c) 1995-2000 University College London
# All rights reserved.
#

proc mbus_recv {cmd args} {
  if [string match [info procs [lindex mbus_recv_$cmd 0]] cb_recv_$cmd] {
    #eval mbus_recv_$cmd $args
  }
}

# The transcoder UI comprises three frames:
#	.l is the "left" side of the transcoder
#	.r is the "right" side of the transcoder
#	.c is for common controls

frame .l
frame .r
frame .c

