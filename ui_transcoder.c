char ui_transcoder[] = "\
#\n\
# ui_transcoder.tcl\n\
#\n\
# Copyright (c) 1995-2000 University College London\n\
# All rights reserved.\n\
#\n\
\n\
proc mbus_recv {cmd args} {\n\
  if [string match [info procs [lindex mbus_recv_$cmd 0]] cb_recv_$cmd] {\n\
    #eval mbus_recv_$cmd $args\n\
  }\n\
}\n\
\n\
# The transcoder UI comprises three frames:\n\
#	.l is the \"left\" side of the transcoder\n\
#	.r is the \"right\" side of the transcoder\n\
#	.c is for common controls\n\
\n\
frame .l\n\
frame .r\n\
frame .c\n\
\n\
";
