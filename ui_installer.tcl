set f_title    "Helvetica 14 bold"
set f_friendly "Helvetica 12"
set f_legal    "Courier 12"

frame     .t   -relief sunken
label     .t.1 -font $f_title    -text "Robust-Audio Tool"
frame     .m
text      .m.t -height 30 -width 80 -yscrollcommand ".m.s set" -relief flat -bg white -font $f_legal
scrollbar .m.s -command ".m.t yview" -relief flat
frame     .b
button    .b.reject -font $f_friendly -command do_reject -text "I do not accept these conditions"
button    .b.accept -font $f_friendly -command do_accept -text "I have read and accept these conditions"

pack .t        -fill x
pack .t.1      -fill x
pack .m.s      -anchor w -side left -expand 0 -fill y
pack .m.t      -anchor w -side left -expand 1 -fill both
pack .m        -expand 1 -fill both
pack .b.reject -side left -expand 1 -fill x
pack .b.accept -side left -expand 1 -fill x
pack .b        -fill x

.m.t insert end \
{Copyright (C) 1995-2000 University College London
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, is permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:

     This product includes software developed by the Computer Science
     Department at University College London.

4. Neither the name of the University nor of the Department may be used
   to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This software is derived, in part, from publically available
and contributed source code with the following copyright:

Copyright (C) 1991-1993 Regents of the University of California
Copyright (C) 1992 Stichting Mathematisch Centrum, Amsterdam
Copyright (C) 1991-1992 RSA Data Security, Inc
Copyright (C) 1992 Jutta Degener and Carsten Bormann, TU Berlin
Copyright (C) 1994 Paul Stewart
Copyright (C) 1996 Regents of the University of California
Copyright (C) 2000 Nortel Networks
Copyright (C) 2000 Argonne National Laboratory

This product includes software developed by the Computer
Systems Engineering Group and by the Network Research Group
at Lawrence Berkeley Laboratory.

The WB-ADPCM algorithm was developed by British Telecommunications
plc.  Permission has been granted to use it for non-commercial
research and development projects.  BT retain the intellectual
property rights to this algorithm.

Encryption features of this software use the RSA Data
Security, Inc. MD5 Message-Digest Algorithm.
}

wm resizable . 0 0

proc do_reject {} {
	destroy .
}

