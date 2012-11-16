#
# Copyright (c) 1995-2002 University College London
# All rights reserved.
#
# $Revision: 4547 $
#
# Full terms and conditions of the copyright appear below.
#

package require Tk

if {$::tk_version > 8.4} {
    package require Ttk
    ttk::style theme use clam
}

set script_error [ catch {

wm withdraw .

if {[string compare [info commands registry] "registry"] == 0} {
    set win32 1
} else {
    set win32 0
    if {$::tk_version < 8.5} {
	option add *Menu*selectColor		forestgreen
	option add *Radiobutton*selectColor	forestgreen
	option add *Checkbutton*selectColor	forestgreen
	option add *Entry.background		gray70
    }
}

set statsfont     [font actual {helvetica 10}]
set compfont      [font actual {helvetica 10}]
set titlefont     [font actual {helvetica 10}]
set infofont      [font actual {helvetica 10}]
set smallfont     [font actual {helvetica  8}]
set verysmallfont [font actual {helvetica  8}]

set speaker_highlight white

option add *Entry.relief       sunken
option add *borderWidth        1
option add *highlightThickness 0
option add *font               $infofont
option add *Menu*tearOff       0

set V(class) "Mbone Applications"
set V(app)   "rat"

set iht			16
set iwd 		280
set cancel_info_timer 	0
set num_ssrc		0
set fw			.l.t.list.f
set iports             [list]
set oports             [list]
set bps_in              "0.0 b/s"
set bps_out             $bps_in

# Sliders use scale widget and this invokes it's -command option when
# it is created (doh!)  We don't know what gain and volume settings
# should be before we've received info from the audio engine.
# Don't send our initial value if we've not heard anything from engine
# first.  Otherwise we get catch-22 with both engine and ui queuing
# messages of gains to each other and often UI wins, which it never
# should do.

set received(gain)      0
set received(volume)    0

###############################################################################
#
# Debugging Functions
#
###############################################################################

# tvar - trace variable callback for watching changes to variables
# e.g. to watch write events to myVar use:
#                                          trace variable myVar w tvar

proc tvar {name element op} {
# Compose variable name and print out change
    if {$element != ""} {
	set name ${name}($element)
    }
    upvar $name x
    puts stderr "Variable $name set to $x."

# Unravel stack and display
    puts "Stack trace:"
    set depth [info level]
    set l 0
    while {$l < $depth} {
	puts stderr "\tLevel $l: [info level $l]"
	incr l
    }
}

###############################################################################

# trace variable gain w tvar

proc init_source {ssrc} {
	global CNAME NAME EMAIL LOC PHONE TOOL NOTE PRIV SSRC num_ssrc
	global CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP \
               JITTER LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED BUFFER_SIZE \
	       PLAYOUT_DELAY GAIN MUTE SKEW SPIKE_EVENTS SPIKE_TOGED RTT

	# This is a debugging test -- old versions of the mbus used the
	# cname to identify participants, whilst the newer version uses
	# the ssrc.  This check detects if old style commands are being
	# used and raises an error if so.
	if [regexp {.*@[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+} "$ssrc"] {
		error "ssrc $ssrc invalid"
	}

	if {[array names INDEX $ssrc] != [list $ssrc]} {
		# This is a source we've not seen before...
		set         CNAME($ssrc) ""
		set          NAME($ssrc) $ssrc
		set         EMAIL($ssrc) ""
		set         PHONE($ssrc) ""
		set           LOC($ssrc) ""
		set          TOOL($ssrc) ""
		set          NOTE($ssrc) ""
		set          PRIV($ssrc) ""
		set         CODEC($ssrc) unknown
		set          GAIN($ssrc) 1.0
		set          MUTE($ssrc) 0
		set      DURATION($ssrc) ""
		set   BUFFER_SIZE($ssrc) 0
		set PLAYOUT_DELAY($ssrc) 0
		set          SKEW($ssrc) 1.000
		set  SPIKE_EVENTS($ssrc) 0
		set   SPIKE_TOGED($ssrc) 0
		set           RTT($ssrc) ""
		set    PCKTS_RECV($ssrc) 0
		set    PCKTS_LOST($ssrc) 0
		set    PCKTS_MISO($ssrc) 0
		set     PCKTS_DUP($ssrc) 0
		set        JITTER($ssrc) 0
		set     JIT_TOGED($ssrc) 0
		set    LOSS_TO_ME($ssrc) 0
		set  LOSS_FROM_ME($ssrc) 0
		set  HEARD_LOSS_TO_ME($ssrc) 0
		set  HEARD_LOSS_FROM_ME($ssrc) 0
		set         INDEX($ssrc) $num_ssrc
		set          SSRC($ssrc) $ssrc
		incr num_ssrc
		chart::add   $ssrc
	}
}

proc window_plist {ssrc} {
    	global fw
	regsub -all {@|\.} $ssrc {-} foo
	return $fw.source-$foo
}

proc window_stats {ssrc} {
	regsub -all {[\. ]} $ssrc {-} foo
	return .stats$foo
}

# Commands to send message over the conference bus...
proc output_mute {state} {
    mbus_send "R" "audio.output.mute" "$state"
    if {$state} {
    	bargraph::disable .r.c.rx.au.pow.bar
    } else {
        bargraph::enable .r.c.rx.au.pow.bar
    }
}

proc input_mute {state} {
    mbus_send "R" "audio.input.mute" "$state"
    if {$state} {
    	bargraph::disable .r.c.tx.au.pow.bar
    } else {
        bargraph::enable .r.c.tx.au.pow.bar
    }
}

proc set_vol {new_vol} {
    global volume received
    set volume $new_vol
    if {$received(volume)} {
	mbus_send "R" "audio.output.gain" $volume
    }
}

proc set_gain {new_gain} {
    global gain received
    set gain $new_gain
    if {$received(gain)} {
	mbus_send "R" "audio.input.gain" $gain
    }
}

proc toggle_iport {} {
    global iport iports

    set len [llength $iports]
# lsearch returns -1 if not found, index otherwise
    set idx [lsearch -exact $iports $iport]

    if {$idx != -1} {
	incr idx
	set idx [expr $idx % $len]
	set port [lindex $iports $idx]
	mbus_send "R" "audio.input.port" [mbus_encode_str $port]
    }
}

proc incr_port {varname listname delta} {
    upvar $varname  port
    upvar $listname ports
    set len [llength $ports]
    # lsearch returns -1 if not found, index otherwise
    set idx [lsearch -exact $ports $port]

    if {$idx != -1} {
	incr idx $delta
	if {$idx < 0} {
	    set idx [expr $len - 1]
	} elseif {$idx >= $len} {
	    set idx 0
	}
	set port [lindex $ports $idx]
# yuechh!
	if {$varname == "iport"} {
	    mbus_send "R" "audio.input.port" [mbus_encode_str $port]
	} else {
	    mbus_send "R" "audio.output.port" [mbus_encode_str $port]
	}
    }
}

proc mbus_heartbeat {} {
}

#############################################################################################################
# Reception of Mbus messages...

proc mbus_recv {cmnd args} {
	# This is not the most efficient way of doing this, since we could call mbus_recv_...
	# directly from the C code. It does, however, make it explicit which Mbus commands we
	# understand.
	switch $cmnd {
		mbus.waiting			{eval mbus_recv_mbus.waiting $args}
		mbus.go				{eval mbus_recv_mbus.go $args}
		mbus.hello			{eval mbus_recv_mbus.hello $args}
		mbus.quit  			{eval mbus_recv_mbus.quit $args}
		tool.rat.sampling.supported 	{eval mbus_recv_tool.rat.sampling.supported $args}
		tool.rat.converter  		{eval mbus_recv_tool.rat.converter $args}
		tool.rat.converters.flush 	{eval mbus_recv_tool.rat.converters.flush $args}
		tool.rat.converters.add 	{eval mbus_recv_tool.rat.converters.add $args}
		tool.rat.repairs.flush		{eval mbus_recv_tool.rat.repairs.flush $args}
		tool.rat.repairs.add		{eval mbus_recv_tool.rat.repairs.add $args}
		tool.rat.powermeter		{eval mbus_recv_tool.rat.powermeter $args}
		tool.rat.agc  			{eval mbus_recv_tool.rat.agc $args}
		tool.rat.format.in              {eval mbus_recv_tool.rat.format.in $args}
		tool.rat.codec  		{eval mbus_recv_tool.rat.codec $args}
		tool.rat.codecs.flush           {eval mbus_recv_tool.rat.codecs.flush $args}
		tool.rat.codecs.add             {eval mbus_recv_tool.rat.codecs.add $args}
		tool.rat.rate  			{eval mbus_recv_tool.rat.rate $args}
		tool.rat.lecture.mode  		{eval mbus_recv_tool.rat.lecture.mode $args}
		tool.rat.audio.buffered  	{eval mbus_recv_tool.rat.audio.buffered $args}
		tool.rat.audio.delay    	{eval mbus_recv_tool.rat.audio.delay $args}
		tool.rat.audio.skew     	{eval mbus_recv_tool.rat.audio.skew $args}
		tool.rat.spike.events    	{eval mbus_recv_tool.rat.spike.events $args}
		tool.rat.spike.toged     	{eval mbus_recv_tool.rat.spike.toged $args}
		tool.rat.silence                {eval mbus_recv_tool.rat.silence $args}
		tool.rat.silence.threshold      {eval mbus_recv_tool.rat.silence.threshold $args}
		audio.3d.enabled  		{eval mbus_recv_audio.3d.enabled $args}
		audio.3d.azimuth.min            {eval mbus_recv_audio.3d.azimuth.min $args}
		audio.3d.azimuth.max            {eval mbus_recv_audio.3d.azimuth.max $args}
		audio.3d.filter.types           {eval mbus_recv_audio.3d.filter.types   $args}
		audio.3d.filter.lengths         {eval mbus_recv_audio.3d.filter.lengths $args}
		audio.3d.user.settings          {eval mbus_recv_audio.3d.user.settings  $args}
		tool.rat.playout.limit          {eval mbus_recv_tool.rat.playout.limit $args}
		tool.rat.playout.min            {eval mbus_recv_tool.rat.playout.min   $args}
		tool.rat.playout.max            {eval mbus_recv_tool.rat.playout.max   $args}
		tool.rat.echo.suppress          {eval mbus_recv_tool.rat.echo.suppress $args}
		tool.rat.loopback.gain          {eval mbus_recv_tool.rat.loopback.gain      $args}
		tool.rat.bps.in                 {eval mbus_recv_tool.rat.bps.in        $args}
		tool.rat.bps.out                {eval mbus_recv_tool.rat.bps.out       $args}
		audio.suppress.silence  	{eval mbus_recv_audio.suppress.silence $args}
		audio.channel.coding  		{eval mbus_recv_audio.channel.coding $args}
		audio.channel.repair 		{eval mbus_recv_audio.channel.repair $args}
		audio.devices.flush             {eval mbus_recv_audio_devices_flush $args}
		audio.devices.add               {eval mbus_recv_audio_devices_add $args}
		audio.device                    {eval mbus_recv_audio_device $args}
		audio.input.gain  		{eval mbus_recv_audio.input.gain $args}
		audio.input.port  		{eval mbus_recv_audio.input.port $args}
		audio.input.ports.add           {eval mbus_recv_audio.input.ports.add $args}
		audio.input.ports.flush         {eval mbus_recv_audio.input.ports.flush $args}
		audio.input.mute  		{eval mbus_recv_audio.input.mute $args}
		audio.input.powermeter  	{eval mbus_recv_audio.input.powermeter $args}
		audio.output.gain  		{eval mbus_recv_audio.output.gain $args}
		audio.output.port  		{eval mbus_recv_audio.output.port $args}
		audio.output.ports.add          {eval mbus_recv_audio.output.ports.add $args}
		audio.output.ports.flush        {eval mbus_recv_audio.output.ports.flush $args}
		audio.output.mute  		{eval mbus_recv_audio.output.mute $args}
		audio.output.powermeter  	{eval mbus_recv_audio.output.powermeter $args}
		audio.file.play.ready   	{eval mbus_recv_audio.file.play.ready   $args}
		audio.file.play.alive   	{eval mbus_recv_audio.file.play.alive $args}
		audio.file.record.ready 	{eval mbus_recv_audio.file.record.ready $args}
		audio.file.record.alive 	{eval mbus_recv_audio.file.record.alive $args}
		session.title  			{eval mbus_recv_session.title $args}
		rtp.addr  			{eval mbus_recv_rtp.addr $args}
		rtp.ssrc  			{eval mbus_recv_rtp.ssrc $args}
		rtp.source.exists  		{eval mbus_recv_rtp.source.exists $args}
		rtp.source.remove  		{eval mbus_recv_rtp.source.remove $args}
		rtp.source.cname  		{eval mbus_recv_rtp.source.cname $args}
		rtp.source.name  		{eval mbus_recv_rtp.source.name $args}
		rtp.source.email  		{eval mbus_recv_rtp.source.email $args}
		rtp.source.phone  		{eval mbus_recv_rtp.source.phone $args}
		rtp.source.loc  		{eval mbus_recv_rtp.source.loc $args}
		rtp.source.tool  		{eval mbus_recv_rtp.source.tool $args}
		rtp.source.note  		{eval mbus_recv_rtp.source.note $args}
		rtp.source.priv  		{eval mbus_recv_rtp.source.priv $args}
		rtp.source.codec  		{eval mbus_recv_rtp.source.codec $args}
		rtp.source.packet.duration  	{eval mbus_recv_rtp.source.packet.duration $args}
		rtp.source.packet.loss  	{eval mbus_recv_rtp.source.packet.loss $args}
		rtp.source.reception  		{eval mbus_recv_rtp.source.reception $args}
		rtp.source.active  		{eval mbus_recv_rtp.source.active $args}
		rtp.source.inactive		{eval mbus_recv_rtp.source.inactive $args}
		rtp.source.mute			{eval mbus_recv_rtp.source.mute $args}
		rtp.source.gain			{eval mbus_recv_rtp.source.gain $args}
		rtp.source.rtt			{eval mbus_recv_rtp.source.rtt $args}
		rtp.source.app.site     {eval mbus_recv_rtp.source.app.site $args}
		security.encryption.key 	{eval mbus_recv_security.encryption.key $args}
		default				{bgerror "Unknown mbus command $cmnd"}
	}
}

proc mbus_recv_mbus.waiting {condition} {
	if {$condition == "rat.ui.init"} {
		mbus_send "U" "mbus.go" [mbus_encode_str rat.ui.init]
	}
}

proc mbus_recv_mbus.go {condition} {
}

proc mbus_recv_mbus.hello {} {
	# Ignore...
}

proc change_freq {new_freq} {
    global freq

    if {$freq != $new_freq} {
	set freq $new_freq
	update_channels_displayed
	update_codecs_displayed
	reset_encodings
    }
}

proc change_channels {new_channels} {
    global ichannels
    if {$ichannels != $new_channels} {
	set ichannels $new_channels
	update_codecs_displayed
	reset_encodings
    }
}

proc update_channels_displayed {} {
    global freq channel_support

    set m1 .prefs.pane.audio.dd.sampling.ch_in.mb.m
    $m1 delete 0 last
    set s [lsearch -glob $channel_support *$freq*]

    foreach i [lrange [split [lindex $channel_support $s] ","] 1 2] {
	 $m1 add command -label "$i" -command "change_channels $i"
    }
}

proc mbus_recv_tool.rat.sampling.supported {arg} {
    global freq channel_support

    #clear away old state of channel support
    if [info exists channel_support] {
	unset channel_support
    }

    set freqs [list]
    set channel_support [list]

    .prefs.pane.audio.dd.sampling.freq.mb.m delete 0 last

    set mode [split $arg]
    foreach m $mode {
	lappend channel_support $m
	set support [split $m ","]
	set f [lindex $support 0]
	lappend freqs $f
	.prefs.pane.audio.dd.sampling.freq.mb.m add command -label $f -command "change_freq $f"
    }
    set freq [lindex $freqs 0]
    update_channels_displayed
}

# CODEC HANDLING ##############################################################

set codecs {}
set prencs  {}
set secencs {}
set layerencs {}
set layerenc 1

proc codec_get_name {nickname freq channels} {
    global codecs codec_nick_name codec_rate codec_channels

    foreach {c} $codecs {
	if {$codec_nick_name($c)    == $nickname && \
		$codec_rate($c)     == $freq && \
		$codec_channels($c) == $channels} {
	    return $c
	}
    }
}

proc codecs_loosely_matching {freq channels} {
    global codecs codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc

    set x {}

    foreach {c} $codecs {
	if {$codec_channels($c) == $channels && \
	$codec_rate($c) == $freq && \
	$codec_pt($c) != "-" } {
	    lappend x $c
	}
    }

    return $x
}

proc codecs_matching {freq channels blocksize} {
    global codec_block_size
    set codecs [codecs_loosely_matching $freq $channels]

    set x {}

    foreach {c} $codecs {
	if {$codec_block_size($c) == $blocksize} {
	    lappend x $c
	}
    }
    return $x
}

proc mbus_recv_tool.rat.codecs.flush {args} {
    global codecs

    set codecs {}

    set cvars [list codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc codec_caps codec_layers ]
    foreach c $cvars {
	global $c
	upvar 0 $c v
	if {[info exists v]} {
	    unset v
	}
    }
}

proc mbus_recv_tool.rat.codecs.add {args} {
    catch {
	global codecs codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc codec_caps codec_layers

	set name [lindex $args 1]
	if {[lsearch $codecs $name] == -1} {
	    lappend codecs $name
	}
	set codec_pt($name)         [lindex $args 0]
	set codec_nick_name($name)  [lindex $args 2]
	set codec_channels($name)   [lindex $args 3]
	set codec_rate($name)       [lindex $args 4]
	set codec_block_size($name) [lindex $args 5]
	set codec_state_size($name) [lindex $args 6]
	set codec_data_size($name)  [lindex $args 7]
	set codec_desc($name)       [lindex $args 8]
	set codec_caps($name)       [lindex $args 9]
	set codec_layers($name)		[lindex $args 10]
	set stackup ""
    } details_error

    if { $details_error != "" } {
		bgerror "Error: $details_error"
		destroy .
		exit -1
    }
}

proc update_primary_list {arg} {
    # We now have a list of codecs which this RAT supports...
    global prenc prencs

    .prefs.pane.transmission.dd.pri.m.menu delete 0 last
    set prencs {}

    set codecs [split $arg]
    foreach c $codecs {
	.prefs.pane.transmission.dd.pri.m.menu    add command -label $c -command "set prenc $c; update_codecs_displayed"
	lappend prencs $c
    }

    if {[lsearch $codecs $prenc] == -1} {
	#primary is not on list
	set prenc [lindex $codecs 0]
    }
}

proc update_redundancy_list {arg} {
    global secenc secencs

    .prefs.pane.transmission.cc.red.fc.m.menu delete 0 last
    set secencs {}

    set codecs [split $arg]
    foreach c $codecs {
	.prefs.pane.transmission.cc.red.fc.m.menu add command -label $c -command "set secenc \"$c\""
	lappend secencs $c
    }
    if {[lsearch $codecs $secenc] == -1} {
	#primary is not on list
	set secenc [lindex $codecs 0]
    }
}

proc update_layer_list {arg} {
    global layerenc layerencs

    .prefs.pane.transmission.cc.layer.fc.m.menu delete 0 last
    set layerencs {}

    for {set i 1} {$i <= $arg} {incr i} {
	.prefs.pane.transmission.cc.layer.fc.m.menu add command -label $i -command "set layerenc \"$i\""
	lappend layerencs $i
	}
	if {$layerenc > $arg} {
	#new codec doesn't support as many layers
	set layerenc $arg
	}
}

proc reset_encodings {} {
    global prenc prencs secenc secencs layerenc layerencs
    set prenc  [lindex $prencs 0]
    set secenc [lindex $secencs 0]
	set layerenc [lindex $layerencs 0]
}

proc update_codecs_displayed { } {
    global freq ichannels codec_nick_name prenc codec_block_size codec_caps codec_layers

    if {[string match $ichannels Mono]} {
	set sample_channels 1
    } else {
	set sample_channels 2
    }

    set sample_rate [string trimright $freq -kHz]
    set sample_rate [expr $sample_rate * 1000]
    if {[expr $sample_rate % 11000] == 0} {
	set sample_rate [expr $sample_rate * 11025 / 11000]
    }

    set long_names [codecs_loosely_matching $sample_rate $sample_channels]

    set friendly_names {}
    foreach {n} $long_names {
	# only interested in codecs that can (e)ncode
	if {[string first $codec_caps($n) ncode]} {
	    lappend friendly_names $codec_nick_name($n)
	}
    }

    update_primary_list $friendly_names

    set long_name [codec_get_name $prenc $sample_rate $sample_channels]
    set long_names [codecs_matching $sample_rate $sample_channels $codec_block_size($long_name)]

    set friendly_names {}
    set found 0
    foreach {n} $long_names {
	# Only display codecs of same or lower order as primary in primary list
	if {$codec_nick_name($n) == $prenc} {
	    set found 1
	}
	if {$found} {
	    if {[string first $codec_caps($n) ncode]} {
		lappend friendly_names $codec_nick_name($n)
	    }
	}
    }

    update_redundancy_list $friendly_names

	# Assume that all codecs of one type support the same number of layers
	foreach {n} $long_names {
	if {$codec_nick_name($n) == $prenc} {
	break
	}
	}
	update_layer_list $codec_layers($n)
}

proc change_sampling { } {
    update_channels_displayed
    update_codecs_displayed
}

###############################################################################

proc mbus_recv_tool.rat.converters.flush {} {
    .prefs.pane.reception.r.ms.menu delete 0 last
}

proc mbus_recv_tool.rat.converters.add {arg} {
    global convert_var
    .prefs.pane.reception.r.ms.menu add command -label "$arg" -command "set convert_var \"$arg\""
}

proc mbus_recv_tool.rat.converter {arg} {
    global convert_var
    set convert_var $arg
}

proc mbus_recv_tool.rat.repairs.flush {} {
    .prefs.pane.reception.r.m.menu delete 0 last
}


proc mbus_recv_tool.rat.repairs.add {arg} {
    global repair_var
    .prefs.pane.reception.r.m.menu add command -label "$arg" -command "set repair_var \"$arg\""
}

proc mbus_recv_audio_devices_flush {} {
    .prefs.pane.audio.dd.device.mdev.menu delete 0 last
}

proc mbus_recv_audio_devices_add {arg} {
    global audio_device

    .prefs.pane.audio.dd.device.mdev.menu add command -label "$arg" -command "set audio_device \"$arg\""

    set len [string length "$arg"]
    set curr [.prefs.pane.audio.dd.device.mdev cget -width]

    if {$len > $curr} {
	.prefs.pane.audio.dd.device.mdev configure -width $len
    }
}

proc mbus_recv_audio_device {arg} {
	global audio_device
	set audio_device $arg
}

proc mbus_recv_tool.rat.powermeter {arg} {
	global meter_var
	set meter_var $arg
}

proc mbus_recv_tool.rat.agc {arg} {
  global agc_var
  set agc_var $arg
}

proc mbus_recv_security.encryption.key {new_key} {
      global key_var key
      set key     $new_key

      if { [string length $key]!=0 } {
	set key_var 1
      } else {
	set key_var 0
      }
      update_security_indicator
}

proc mbus_recv_tool.rat.format.in {arg} {
    global freq ichannels
#expect arg to be <sample_type>,<sample rate>,<mono/stereo>
    set e [split $arg ","]

    set freq      [lindex $e 1]
    set ichannels [lindex $e 2]
}

proc mbus_recv_tool.rat.codec {arg} {
  global prenc
  set prenc $arg
}

proc mbus_recv_tool.rat.rate {arg} {
    global upp
    set upp $arg
}

proc mbus_recv_audio.channel.coding {args} {
    global channel_var secenc red_off int_units int_gap prenc layerenc

    set channel_var [lindex $args 0]

    switch [string tolower $channel_var] {
    	redundancy {
		set secenc  [lindex $args 1]
		set red_off [lindex $args 2]
	}
	interleaved {
		set int_units [lindex $args 1]
		set int_gap   [lindex $args 2]
	}
	layering {
#		should we be playing with primary encoding?
#		set prenc	  [lindex $args 1]
		set layerenc  [lindex $args 2]
	}
    }
}

proc mbus_recv_audio.channel.repair {arg} {
  global repair_var
  set repair_var $arg
}

proc mbus_recv_audio.input.powermeter {level} {
	bargraph::setHeight .r.c.tx.au.pow.bar [expr ($level * $bargraph::totalHeight) / 100]
}

proc mbus_recv_audio.output.powermeter {level} {
	bargraph::setHeight .r.c.rx.au.pow.bar  [expr ($level * $bargraph::totalHeight) / 100]
}

proc mbus_recv_audio.input.gain {new_gain} {
    global received gain
    set received(gain) 1
    set gain $new_gain
}

proc mbus_recv_audio.input.ports.flush {} {
    global iports
    set iports [list]
}

proc mbus_recv_audio.input.ports.add {port} {
    global iports
    lappend iports "$port"
}

proc mbus_recv_audio.input.port {port} {
    global iport
    set    iport $port
}

proc mbus_recv_audio.input.mute {val} {
    global in_mute_var
    set in_mute_var $val
    if {$val} {
        bargraph::disable .r.c.tx.au.pow.bar
    } else {
        bargraph::enable .r.c.tx.au.pow.bar
    }
}

proc mbus_recv_audio.output.gain {new_gain} {
    global received volume
    set received(volume) 1
    set volume $new_gain
}

proc mbus_recv_audio.output.port {port} {
    global oport
    set oport $port
}

proc mbus_recv_audio.output.ports.flush {} {
    global oports
    set oports [list]
}

proc mbus_recv_audio.output.ports.add {port} {
    global oports
    lappend oports "$port"
}

proc mbus_recv_audio.output.mute {val} {
    global out_mute_var
    set out_mute_var $val
}

proc mbus_recv_session.title {title} {
    global session_title tool_name
    set session_title \"$title\"
    wm title . "$tool_name: $title"
    wm deiconify .
}

proc mbus_recv_rtp.addr {addr rx_port tx_port ttl} {
    global session_address group_addr g_rx_port g_tx_port g_ttl
    set g_rx_port $rx_port
    set g_tx_port $tx_port
    set g_ttl $ttl
    set group_addr $addr
    set session_address "$addr/$rx_port/$ttl"
}

proc mbus_recv_tool.rat.lecture.mode {mode} {
	global lecture_var
	set lecture_var $mode
}


proc mbus_recv_audio.suppress.silence {mode} {
    global silence_var

#    if {$mode == 0} {
#	set silence_var "Off"
#    } else {
#	set silence_var "Automatic"
#    }
#    puts "audio.suppress.silence silence_var $silence_var"
}

proc mbus_recv_tool.rat.silence {mode} {
    global silence_var
    switch -regexp $mode {
	^[Oo].* { set silence_var Off }
	^[Aa].* { set silence_var Automatic }
	^[Mm].* { set silence_var Manual }
    }
}

proc mbus_recv_tool.rat.silence.threshold {thresh} {
    global manual_silence_thresh
    set manual_silence_thresh $thresh
}

proc mbus_recv_rtp.ssrc {ssrc} {
	global my_ssrc

	set my_ssrc $ssrc
	init_source $ssrc
	ssrc_update $ssrc
#        puts "Got my_ssrc $ssrc"
}

proc mbus_recv_rtp.source.exists {ssrc} {
	init_source $ssrc
	chart::add $ssrc
	ssrc_update $ssrc
}

proc mbus_recv_rtp.source.cname {ssrc cname} {
	global CNAME NAME SSRC
	init_source $ssrc
	set CNAME($ssrc) $cname
	if {[string compare $NAME($ssrc) $SSRC($ssrc)] == 0} {
		set NAME($ssrc) $cname
	}
	chart::setlabel $ssrc $NAME($ssrc)
	ssrc_update $ssrc
}

proc mbus_recv_rtp.source.name {ssrc name} {
	global NAME
	init_source $ssrc
	set NAME($ssrc) $name
	chart::setlabel $ssrc $name
	ssrc_update $ssrc
}

proc mbus_recv_rtp.source.email {ssrc email} {
	global EMAIL
	init_source $ssrc
	set EMAIL($ssrc) $email
}

proc mbus_recv_rtp.source.phone {ssrc phone} {
	global PHONE
	init_source $ssrc
	set PHONE($ssrc) $phone
}

proc mbus_recv_rtp.source.loc {ssrc loc} {
	global LOC
	init_source $ssrc
	set LOC($ssrc) $loc
}

proc mbus_recv_rtp.source.priv {ssrc priv} {
	global PRIV
	init_source $ssrc
	set PRIV($ssrc) $priv
}

proc mbus_recv_rtp.source.tool {ssrc tool} {
	global TOOL my_ssrc tool_name
	init_source $ssrc
	set TOOL($ssrc) $tool
	if {[info exists my_ssrc] && [string compare $ssrc $my_ssrc] == 0} {
	    global tool_name
	    # tool name looks like RAT x.x.x platform ....
	    # lose the platform stuff
	    set tool_frag [split $tool]
	    set tool_name "[lindex $tool_frag 0] [lindex $tool_frag 1]"
	}
}

proc mbus_recv_rtp.source.note {ssrc note} {
	global NOTE
	init_source $ssrc
	set NOTE($ssrc) $note
}


proc mbus_recv_rtp.source.codec {ssrc codec} {
	global CODEC
	init_source $ssrc
	set CODEC($ssrc) $codec
}

proc mbus_recv_rtp.source.gain {ssrc gain} {
	global GAIN
	init_source $ssrc
	set GAIN($ssrc) $gain
}

proc mbus_recv_rtp.source.packet.duration {ssrc packet_duration} {
	global DURATION
	init_source $ssrc
	set DURATION($ssrc) $packet_duration
}

proc mbus_recv_tool.rat.audio.buffered {ssrc buffered} {
        global BUFFER_SIZE
        init_source $ssrc
        set BUFFER_SIZE($ssrc) $buffered
}

proc mbus_recv_tool.rat.audio.delay {ssrc len} {
        global PLAYOUT_DELAY
        init_source $ssrc
        set PLAYOUT_DELAY($ssrc) $len
}

proc mbus_recv_tool.rat.audio.skew {ssrc skew} {
        global SKEW
        init_source $ssrc
        set SKEW($ssrc) [format "%.3f" $skew]
}

proc mbus_recv_tool.rat.spike.events {ssrc events} {
    global SPIKE_EVENTS
    init_source $ssrc
    set SPIKE_EVENTS($ssrc) $events
}

proc mbus_recv_tool.rat.spike.toged {ssrc toged} {
    global SPIKE_TOGED
    init_source $ssrc
    set SPIKE_TOGED($ssrc) $toged
}

proc mbus_recv_rtp.source.rtt {ssrc rtt} {
    global RTT
    init_source $ssrc
    set RTT($ssrc) $rtt
}

proc mbus_recv_audio.3d.enabled {mode} {
	global 3d_audio_var
	set 3d_audio_var $mode
}

proc mbus_recv_audio.3d.azimuth.min {min} {
    global 3d_azimuth
    set 3d_azimuth(min) $min
}

proc mbus_recv_audio.3d.azimuth.max {max} {
    global 3d_azimuth
    set 3d_azimuth(max) $max
}

proc mbus_recv_audio.3d.filter.types {args} {
    global 3d_filters
    set 3d_filters [split $args ","]
}

proc mbus_recv_audio.3d.filter.lengths {args} {
    global 3d_filter_lengths
    set 3d_filter_lengths [split $args ","]
}

proc mbus_recv_audio.3d.user.settings {args} {
    global filter_type filter_length azimuth
    set ssrc                 [lindex $args 0]
    set filter_type($ssrc)   [lindex $args 1]
    set filter_length($ssrc) [lindex $args 2]
    set azimuth($ssrc)       [lindex $args 3]
}

proc mbus_recv_rtp.source.packet.loss {dest srce loss} {
	global my_ssrc LOSS_FROM_ME LOSS_TO_ME HEARD_LOSS_FROM_ME HEARD_LOSS_TO_ME
	init_source $srce
	init_source $dest
	chart::setvalue $srce $dest $loss
	if {[info exists my_ssrc] && [string compare $dest $my_ssrc] == 0} {
		set LOSS_TO_ME($srce) $loss
    		set HEARD_LOSS_TO_ME($srce) 1
	}
	if {[info exists my_ssrc] && [string compare $srce $my_ssrc] == 0} {
		set LOSS_FROM_ME($dest) $loss
		set HEARD_LOSS_FROM_ME($dest) 1
	}
	ssrc_update $srce
	ssrc_update $dest
}

proc mbus_recv_rtp.source.reception {ssrc packets_recv packets_lost packets_miso packets_dup jitter jit_tog} {
	global PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER JIT_TOGED
	init_source $ssrc
	set PCKTS_RECV($ssrc) $packets_recv
	set PCKTS_LOST($ssrc) $packets_lost
	set PCKTS_MISO($ssrc) $packets_miso
	set PCKTS_DUP($ssrc)  $packets_dup
	set JITTER($ssrc) $jitter
	set JIT_TOGED($ssrc) $jit_tog
}

proc mbus_recv_rtp.source.active {ssrc} {
	global speaker_highlight
	init_source $ssrc
	ssrc_update $ssrc
	[window_plist $ssrc] configure -background $speaker_highlight
}

proc mbus_recv_rtp.source.inactive {ssrc} {
	init_source $ssrc
	ssrc_update $ssrc
	[window_plist $ssrc] configure -background [.l.t.list cget -bg]
}

proc mbus_recv_rtp.source.remove {ssrc} {
    global loss_to_me_timer loss_from_me_timer num_ssrc
    set did_remove 0

    # Disable updating of loss diamonds. This has to be done before we destroy the
    # window representing the participant, else the background update may try to
    # access a window which has been destroyed...

    catch {after cancel $loss_to_me_timer($ssrc)}
    catch {after cancel $loss_from_me_timer($ssrc)}

    set pvars [list CNAME NAME EMAIL LOC PHONE TOOL NOTE PRIV CODEC DURATION \
	    PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER BUFFER_SIZE \
	    PLAYOUT_DELAY LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED loss_to_me_timer \
	    loss_from_me_timer GAIN MUTE HEARD_LOSS_TO_ME HEARD_LOSS_FROM_ME  \
	    SKEW SPIKE_EVENTS SPIKE_TOGED RTT]

    # safely delete array entries
    foreach i $pvars {
	global $i
	upvar 0 $i v
	if {[info exists v($ssrc)]} {
	    unset v($ssrc)
	    set did_remove 1
	}
    }

    catch [destroy [window_plist $ssrc]]
    chart::remove $ssrc

    set stats_win [window_stats $ssrc]
    if { [winfo exists $stats_win] } {
	destroy $stats_win
    }

    if {$did_remove} {
	incr num_ssrc -1
    }
}

proc mbus_recv_rtp.source.mute {ssrc val} {
	global iht MUTE
	set MUTE($ssrc) $val
	if {$val} {
		[window_plist $ssrc] create line [expr $iht + 2] [expr $iht / 2] 500 [expr $iht / 2] -tags a -width 2.0 -fill gray95
	} else {
		catch [[window_plist $ssrc] delete a]
	}
}

proc mbus_recv_rtp.source.app.site {ssrc val} {
}

proc mbus_recv_audio.file.play.ready {name} {
    global play_file
    set    play_file(name) $name

    if {$play_file(state) != "play"} {
	file_enable_play
    }
}

proc mbus_recv_audio.file.play.alive {alive} {

    global play_file

    if {$alive} {
	after 200 file_play_live
    } else {
	set play_file(state) end
	file_enable_play
    }
}

proc mbus_recv_audio.file.record.ready {name} {
    global rec_file
    set    rec_file(name) $name
    if {$rec_file(state) != "record"} {
	file_enable_record
    }
}

proc mbus_recv_audio.file.record.alive {alive} {
	global rec_file
	if {$alive} {
		after 200 file_rec_live
	} else {
		set rec_file(state) end
		file_enable_record
	}
}

proc mbus_recv_tool.rat.playout.limit {e} {
    global limit_var
    set limit_var $e
}

proc mbus_recv_tool.rat.playout.min {l} {
    global min_var
    set min_var $l
}

proc mbus_recv_tool.rat.playout.max {u} {
    global max_var
    set max_var $u
}

proc mbus_recv_tool.rat.echo.suppress {e} {
    global echo_var
    set echo_var $e
}

proc mbus_recv_tool.rat.loopback.gain {l} {
    global audio_loop_var
    set audio_loop_var $l
}

proc format_bps {bps} {
    if {$bps < 999} {
	set s "b/s"
    } elseif {$bps < 999999} {
	set s "kb/s"
	set bps [expr $bps / 1000.0]
    } else {
	set s "Mb/s"
	set bps [expr $bps / 1000000.0]
    }
    return [format "%.1f %s" $bps $s]
}

proc mbus_recv_tool.rat.bps.in {bps} {
    global bps_in
    set    bps_in [format_bps $bps]
}

proc mbus_recv_tool.rat.bps.out {bps} {
    global bps_out
    set    bps_out [format_bps $bps]
}

proc mbus_recv_mbus.quit {} {
	destroy .
}

#############################################################################################################

proc set_loss_to_me {ssrc loss} {
	global prev_loss_to_me loss_to_me_timer

	catch {after cancel $loss_to_me_timer($ssrc)}
	set loss_to_me_timer($ssrc) [after 7500 catch \"[window_plist $ssrc] itemconfigure h -fill grey\"]

	if {$loss < 5} {
		catch [[window_plist $ssrc] itemconfigure m -fill green]
	} elseif {$loss < 10} {
		catch [[window_plist $ssrc] itemconfigure m -fill orange]
	} elseif {$loss <= 100} {
		catch [[window_plist $ssrc] itemconfigure m -fill red]
	} else {
		catch [[window_plist $ssrc] itemconfigure m -fill grey]
	}
}

proc set_loss_from_me {ssrc loss} {
	global prev_loss_from_me loss_from_me_timer

	catch {after cancel $loss_from_me_timer($ssrc)}
	set loss_from_me_timer($ssrc) [after 7500 catch \"[window_plist $ssrc] itemconfigure h -fill grey\"]

	if {$loss < 5} {
		catch [[window_plist $ssrc] itemconfigure h -fill green]
	} elseif {$loss < 10} {
		catch [[window_plist $ssrc] itemconfigure h -fill orange]
	} elseif {$loss <= 100} {
		catch [[window_plist $ssrc] itemconfigure h -fill red]
	} else {
		catch [[window_plist $ssrc] itemconfigure h -fill grey]
	}
}

proc ssrc_update {ssrc} {
	# This procedure updates the on-screen representation of
	# a participant.
	global NAME LOSS_TO_ME LOSS_FROM_ME HEARD_LOSS_FROM_ME HEARD_LOSS_TO_ME
	global fw iht iwd my_ssrc

	set cw [window_plist $ssrc]

	if {[winfo exists $cw]} {
		$cw itemconfigure t -text $NAME($ssrc)
	} else {
		# Add this participant to the list...
		set thick 0
		set l $thick
		set h [expr $iht / 2 + $thick]
		set f [expr $iht + $thick]
		canvas $cw -width $iwd -height $f -highlightthickness $thick
		$cw create text [expr $f + 2] $h -anchor w -text $NAME($ssrc) -fill black -tag t
		$cw create polygon $l $h $h $l $h $f -outline black -fill grey -tag m
		$cw create polygon $f $h $h $l $h $f -outline black -fill grey -tag h
	        scale ${cw}.gain_scale_$ssrc -showvalue 0 -orient h -from -3 -to +3 -resolution 0.25 -command "ssrc_set_gain $ssrc"
	        $cw create window [expr $iwd + 1] $h -anchor e -window $cw.gain_scale_$ssrc 

		bind $cw <Button-1>         "toggle_stats \"$ssrc\""
		bind $cw <Button-2>         "toggle_mute $cw \"$ssrc\""
		bind $cw <Control-Button-1> "toggle_mute $cw \"$ssrc\""

		if {[info exists my_ssrc] && ([string compare $ssrc $my_ssrc] == 0) && ([pack slaves $fw] != "")} {
			pack $cw -before [lindex [pack slaves $fw] 0] -fill x
		}
		pack $cw -fill x
		fix_scrollbar
	}

	if {[info exists HEARD_LOSS_TO_ME($ssrc)] && $HEARD_LOSS_TO_ME($ssrc) && [info exists my_ssrc] && ($ssrc != $my_ssrc)} {
	    set_loss_to_me $ssrc $LOSS_TO_ME($ssrc)
	}
	if {[info exists HEARD_LOSS_FROM_ME($ssrc)] && $HEARD_LOSS_FROM_ME($ssrc) && [info exists my_ssrc] && ($ssrc != $my_ssrc)} {
	    set_loss_from_me $ssrc $LOSS_FROM_ME($ssrc)
	}
}

proc toggle {varname} {
    upvar 1 $varname local
    set local [expr !$local]
}

proc toggle_plist {} {
	global plist_on
	if {$plist_on} {
		pack .l.t  -side top -fill both -expand 1 -padx 2
	} else {
		pack forget .l.t
	}
	update
	wm deiconify .
}

proc toggle_status {} {
	global status_on
	if {$status_on} {
                pack .s -before .st -side bottom -fill x -padx 2 -pady 0
	} else {
		pack forget .s
	}
	update
	wm deiconify .
}

proc toggle_mute {cw ssrc} {
	global iht
	if {[$cw gettags a] == ""} {
		mbus_send "R" "rtp.source.mute" "[mbus_encode_str $ssrc] 1"
	} else {
		mbus_send "R" "rtp.source.mute" "[mbus_encode_str $ssrc] 0"
	}
}

proc send_gain_and_mute {ssrc} {
	global GAIN MUTE
	mbus_send "R" "rtp.source.gain" "[mbus_encode_str $ssrc] $GAIN($ssrc)"
	mbus_send "R" "rtp.source.mute" "[mbus_encode_str $ssrc] $MUTE($ssrc)"
}

proc send_tone_cmd {} {
    global tonegen

    if {$tonegen} {
	# tone generator turned on (Note A, gain 16767 / 32768)
	mbus_send "R" "tool.rat.tone.start" "440 16767"
    } else {
	mbus_send "R" "tool.rat.tone.stop" ""
    }
}

proc fix_scrollbar {} {
	global iht iwd fw

	set ch [expr $iht * ([llength [pack slaves $fw]] + 2)]
	set bh [winfo height .l.t.scr]
	if {$ch > $bh} {set h $ch} else {set h $bh}
	.l.t.list configure -scrollregion "0.0 0.0 $iwd $h"
}

proc info_timer {} {
	global cancel_info_timer
	if {$cancel_info_timer == 1} {
		set cancel_info_timer 0
	} else {
		update_rec_info
		after 1000 info_timer
	}
}

proc stats_add_field {widget label watchVar} {
    global statsfont
    frame $widget
    label $widget.l -text $label -font $statsfont -anchor w
    label $widget.w -textvariable $watchVar -font $statsfont
    pack $widget -side top -fill x -anchor n
    pack $widget.l -side left  -fill x -expand 1
    pack $widget.w -side right
}

proc ssrc_set_gain {ssrc gain} {
    global GAIN
    set    GAIN($ssrc) [format "%.2f " [expr pow (2, $gain)]]
    send_gain_and_mute $ssrc
}

set 3d_azimuth(min) 0
set 3d_azimuth(max) 0
set 3d_filters        [list "Not Available"]
set 3d_filter_lengths [list "0"]

proc toggle_stats {ssrc} {
    global statsfont
    set win [window_stats $ssrc]
    if {[winfo exists $win]} {
	destroy $win
    } else {
	global stats_pane
	# Window does not exist so create it
	toplevel $win
	frame $win.mf
	pack $win.mf -padx 0 -pady 0
	label $win.mf.l -text "Category:"

	menubutton $win.mf.mb -menu $win.mf.mb.menu -indicatoron 1 -textvariable stats_pane($win) -relief raised -width 16
	pack $win.mf.l $win.mf.mb -side left
	menu $win.mf.mb.menu -tearoff 0
	$win.mf.mb.menu add command -label "Personal Details" -command "set_pane stats_pane($win) $win.df \"Personal Details\""
	$win.mf.mb.menu add command -label "Playout"          -command "set_pane stats_pane($win) $win.df Playout"
	$win.mf.mb.menu add command -label "Decoder"          -command "set_pane stats_pane($win) $win.df Decoder"
	$win.mf.mb.menu add command -label "Audio"            -command "set_pane stats_pane($win) $win.df Audio"
	$win.mf.mb.menu add command -label "3D Positioning"   -command "set_pane stats_pane($win) $win.df \"3D Positioning\""

	set stats_pane($win) "Personal Details"
	frame $win.df
	frame $win.df.personal -relief groove -bd 2
	pack  $win.df -side top -anchor n -expand 1 -fill both
	pack $win.df.personal -expand 1 -fill both

	global NAME EMAIL PHONE LOC NOTE PRIV CNAME TOOL SSRC
	stats_add_field $win.df.personal.1 "Name: "     NAME($ssrc)
	stats_add_field $win.df.personal.2 "Email: "    EMAIL($ssrc)
	stats_add_field $win.df.personal.3 "Phone: "    PHONE($ssrc)
	stats_add_field $win.df.personal.4 "Location: " LOC($ssrc)
	stats_add_field $win.df.personal.5 "Note: "     NOTE($ssrc)
	stats_add_field $win.df.personal.6 "Priv: "     PRIV($ssrc)
	stats_add_field $win.df.personal.7 "Tool: "     TOOL($ssrc)
	stats_add_field $win.df.personal.8 "CNAME: "    CNAME($ssrc)
	stats_add_field $win.df.personal.9 "SSRC: "     SSRC($ssrc)

	frame $win.df.playout -relief groove -bd 2
	global BUFFER_SIZE PLAYOUT_DELAY JITTER JIT_TOGED SKEW
	stats_add_field $win.df.playout.1 "Actual Playout (ms): "     BUFFER_SIZE($ssrc)
	stats_add_field $win.df.playout.2 "Target Playout (ms): "     PLAYOUT_DELAY($ssrc)
	stats_add_field $win.df.playout.3 "Arrival Jitter (ms): "     JITTER($ssrc)
	stats_add_field $win.df.playout.4 "Packets Dropped (jitter):" JIT_TOGED($ssrc)
	stats_add_field $win.df.playout.5 "Spike Events:"             SPIKE_EVENTS($ssrc)
	stats_add_field $win.df.playout.6 "Packets Dropped (spike):"  SPIKE_TOGED($ssrc)
	stats_add_field $win.df.playout.7 "Relative Clock Rate:"      SKEW($ssrc)

	frame $win.df.decoder -relief groove -bd 2
	global CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO \
	       PCKTS_DUP LOSS_FROM_ME LOSS_TO_ME
	stats_add_field $win.df.decoder.1 "Audio encoding: "         CODEC($ssrc)
	stats_add_field $win.df.decoder.2 "Packet duration (ms): "   DURATION($ssrc)
	stats_add_field $win.df.decoder.3 "Loss from me (%): "       LOSS_FROM_ME($ssrc)
	stats_add_field $win.df.decoder.4 "Loss to me (%): "         LOSS_TO_ME($ssrc)
	stats_add_field $win.df.decoder.5 "Packets received: "       PCKTS_RECV($ssrc)
	stats_add_field $win.df.decoder.6 "Packets lost: "           PCKTS_LOST($ssrc)
	stats_add_field $win.df.decoder.7 "Packets misordered: "     PCKTS_MISO($ssrc)
	stats_add_field $win.df.decoder.8 "Packets duplicated: "     PCKTS_DUP($ssrc)
	stats_add_field $win.df.decoder.9 "Round Trip Time (ms):"    RTT($ssrc)

# Audio settings
	global GAIN MUTE
	frame $win.df.audio -relief groove -bd 2
	label $win.df.audio.advice -text "The signal from the participant can\nbe scaled and muted with the controls below."
	pack  $win.df.audio.advice

	checkbutton $win.df.audio.mute -text "Mute" -variable MUTE($ssrc) -command "send_gain_and_mute $ssrc"
	pack $win.df.audio.mute

	frame $win.df.audio.opts
	pack  $win.df.audio.opts -side top
	label $win.df.audio.opts.title -text "Gain"
	scale $win.df.audio.opts.gain_scale -showvalue 0 -orient h -from -3 -to +3 -resolution 0.25 -command "ssrc_set_gain $ssrc"
	label $win.df.audio.opts.gain_text -textvariable GAIN($ssrc) -width 4
	pack  $win.df.audio.opts.title $win.df.audio.opts.gain_scale $win.df.audio.opts.gain_text -side left

	$win.df.audio.opts.gain_scale set [expr log10($GAIN($ssrc)) / log10(2)]

	button $win.df.audio.default -text "Default" -command "set MUTE($ssrc) 0; $win.df.audio.opts.gain_scale set 0.0; send_gain_and_mute $ssrc"
	pack   $win.df.audio.default -side bottom  -anchor e -padx 2 -pady 2

# 3D settings
	# Trigger engine to send details for this participant
	mbus_send "R" "audio.3d.user.settings.request" [mbus_encode_str $ssrc]

	frame $win.df.3d -relief groove -bd 2
	label $win.df.3d.advice -text "These options allow the rendering of the\nparticipant to be altered when 3D\nrendering is enabled."
	checkbutton $win.df.3d.ext -text "3D Audio Rendering" -variable 3d_audio_var
	pack $win.df.3d.advice
	pack $win.df.3d.ext

	frame $win.df.3d.opts
	pack $win.df.3d.opts -side top

	frame $win.df.3d.opts.filters
	label $win.df.3d.opts.filters.l -text "Filter Type:"
	pack $win.df.3d.opts.filters.l -side left -fill x -expand 1 -anchor n
	global 3d_filters 3d_filter_lengths

	global filter_type
	set filter_type($ssrc) [lindex $3d_filters 0]

	set cnt 0
	foreach i $3d_filters {
	    radiobutton $win.df.3d.opts.filters.$cnt \
		    -value "$i" -variable filter_type($ssrc) \
		    -text "$i"
		pack $win.df.3d.opts.filters.$cnt -side top -anchor w
	    incr cnt
	}

	frame $win.df.3d.opts.lengths
	label $win.df.3d.opts.lengths.l -text "Length:"
	pack $win.df.3d.opts.lengths.l -side left -fill x -expand 1 -anchor n

	global filter_length
	set filter_length($ssrc) [lindex $3d_filter_lengths 0]

	set cnt 0
	foreach i $3d_filter_lengths {
	    radiobutton $win.df.3d.opts.lengths.$cnt \
		    -value "$i" -variable filter_length($ssrc) \
		    -text "$i"
	    pack $win.df.3d.opts.lengths.$cnt -side top -anchor w
	    incr cnt
	}
	pack $win.df.3d.opts.filters -side left -expand 1 -anchor n -fill x
	pack $win.df.3d.opts.lengths -side right -expand 1 -anchor n -fill x

	global 3d_azimuth azimuth
	frame $win.df.3d.azi
	label $win.df.3d.azi.lab -text "Azimuth:"
	scale $win.df.3d.azi.sca -from $3d_azimuth(min) -to $3d_azimuth(max) \
		-orient horizontal -variable azimuth($ssrc)
	pack  $win.df.3d.azi
	pack $win.df.3d.azi.lab $win.df.3d.azi.sca -side left

	button $win.df.3d.apply -text "Apply" -command "3d_send_parameters $ssrc"
	pack   $win.df.3d.apply -side bottom  -anchor e -padx 2 -pady 2

# Window Magic
	wm title $win "Participant $NAME($ssrc)"
#	wm resizable $win 1 1
	wm protocol  $win WM_DELETE_WINDOW "destroy $win; 3d_delete_parameters $ssrc"
	constrain_window $win $statsfont 36 22
    }
}

proc 3d_send_parameters {ssrc} {
    global azimuth filter_type filter_length 3d_audio_var

    mbus_send "R" "audio.3d.enabled"   $3d_audio_var
    mbus_send "R" "audio.3d.user.settings" "[mbus_encode_str $ssrc] [mbus_encode_str $filter_type($ssrc)] $filter_length($ssrc) $azimuth($ssrc)"
}

proc 3d_delete_parameters {ssrc} {
    global filter_type filter_length azimuth

# None of these should ever fail, but you can't be too defensive...
    catch {
	unset filter_type($ssrc)
	unset filter_length($ssrc)
	unset azimuth($ssrc)
    }
}

proc do_quit {} {
	catch {
		profile off pdat
		profrep pdat cpu
	}
	destroy .
}

proc toggle_chart {} {
	global matrix_on
	if {$matrix_on} {
		chart::show
	} else {
		chart::hide
	}
}

proc update_security_indicator {} {
 global key_var key icons

 if {$key_var == 1 && [string length $key]!=0 } {
    .r.f.f.padlock configure -image $icons(padlock) -bg lightyellow
    .r.f.f.pad configure -bg lightyellow
    .r.f.f.addr configure -bg lightyellow
  } else {
    .r.f.f.padlock configure -image "" -bg gray70
    .r.f.f.pad configure -bg gray70
    .r.f.f.addr configure -bg gray70 
  }
}

proc valid_rtp_addr {addr rx_port tx_port ttl} {
  global group_addr g_rx_port g_tx_port g_ttl

  if { [string match \[0-9\]* $rx_port] && [string match \[0-9\]* $tx_port] } {
      if { $rx_port > 1023 && $rx_port < 65535 && $tx_port > 1023 && $tx_port < 65535 } {
        set addrl [split $addr .]
        if { [llength $addrl] == 4 } {
          foreach i $addrl {
            if {[string match \[0-9\]* $i]} {
              if { $i < 0 || $i > 255 } { 
				return "\"$group_addr\" $g_rx_port $g_tx_port $g_ttl" 
			  }
            } else {
				return "\"$group_addr\" $g_rx_port $g_tx_port $g_ttl"
			}            
		  }
          if {[string match \[0-9\]* $ttl]} {
              if { $ttl > -1 && $ttl < 256 } { 
		         return "\"$addr\" $rx_port $tx_port $ttl"
	          }
	      }
       }
     }
  }
  return "\"$group_addr\" $g_rx_port $g_tx_port $g_ttl"
}

proc change_rtp_addr {l_session_address} {
  global in_mute_var my_ssrc
  global group_addr g_rx_port g_tx_port g_ttl

  focus .
  
  # Return if session_address unchanged
  if { $l_session_address == "$group_addr/$g_rx_port/$g_ttl" } { return }

  # Create new rtp_addr string to signal to mbus_engine
  set rtp_addrl [ split $l_session_address /]
  switch  [llength $rtp_addrl]  {
    1  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] $g_rx_port $g_tx_port $g_ttl]}
    2  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] [lindex $rtp_addrl 1] [lindex $rtp_addrl 1] $g_ttl]}
    3  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] [lindex $rtp_addrl 1] [lindex $rtp_addrl 1] [lindex $rtp_addrl 2]] }
    4  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] [lindex $rtp_addrl 1] [lindex $rtp_addrl 2] [lindex $rtp_addrl 3]] }
    default 
	   { set rtp_addr "\"$group_addr\" $g_rx_port $g_tx_port $g_ttl"}
  }
  # required "rtp.addr": "\"addr\" src_port dst_port ttl" 
  # Check if new address is valid
  # e.g rtp.addr "224.1.2.3" 1234    1234    16
  #               mcast      rx_port tx_port ttl
  mbus_send "R" "rtp.addr" "$rtp_addr"
}


# Initialise RAT MAIN window
frame .r
frame .l
frame .l.t -relief groove -bd 2
if {$::tk_version < 8.5} {
    scrollbar .l.t.scr -relief flat -highlightthickness 0 -command ".l.t.list yview"
} else {
    ttk::style map TScrollbar -background [list disabled [.l.t cget -bg]]

    ttk::scrollbar .l.t.scr -command ".l.t.list yview"
}
canvas .l.t.list -highlightthickness 0 -bd 0 -relief flat -width $iwd -height 120 -yscrollcommand ".l.t.scr set" -yscrollincrement $iht
frame .l.t.list.f -highlightthickness 0 -bd 0
.l.t.list create window 0 0 -anchor nw -window .l.t.list.f

set icons(file_open) [image create photo -format gif  -data { R0lGODlhEgASAPIAAAAAAICAAMDAwPj8APj8+AAAAAAAAAAAACH5BAEAAAIALAAAAAASABIAAAM7KLrc/jAKQCUDC2N7t6JeA2YDMYDoA5hsWYZk28LfjN4b4AJB7/ue1elHDOl4RKAImQzQcDeOdEp1JAAAO///}]

set icons(file_save) [image create photo -data { R0lGODlhEgASAPEAAAAAAICAAMDAwAAAACH5BAEAAAIALAAAAAASABIAAAI3lI+pywYPY0QgAHbvqVpBamHhNnqlwIkdeoJrZUlPcML0jde0DOnxxHrtJA6frLhC8YiNpnNRAAA7////}]

set icons(padlock) [image create photo -format gif -data { R0lGODlhCgAMAMZaAJRpAJRtAJRtCJRxAJxtAJxxAJx1AKV1AJx9AKV5AKV9AKV9CKWCAK2GAK2GCK2KALWGAHuKjISGjK2OAISKjISKlLWOAISKnISOlLWSCIySnIyWpZSapZyelKWqlKWmraWqnK26xrW+vbW+xr2+tbXDxr3DtefLIb3Dzu/PKcbLtcbPrcbL1s7Xpe/bQtbXpc7T1u/fQt7frffnSvfnUt7jrf/rWvfrc97j597j7+frtf/va//zY/f3a/fzhPf3e//ze+/vvf/zhP/3c+fv7//7c+/3vffzvf/7hPf7nP/3nPf3vff3xv/7nPf/pf//nP/7tf//pf/7vf//tf//xv//zv//1v//3v//5///7////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////yH5BAEKAH8ALAAAAAAKAAwAAAd0gEuCKiIhJEGCgiY5REQ4HzpHSyssIxURIzAgiBwoFC01EiUaNUwbGB0yRh4XGi8ZFhMQDQwKCggLFlBZvFhYV1IGD0lWTUI+UlVKBQ9AVD88NEpTNwMNNlM7MzE9TS4EDClOUU9IRUMnAA62CQcDAQEAAoEAOw==}]


frame .r.f -relief groove -bd 2 -width $iwd
frame .r.f.f -relief sunk
label .r.f.f.padlock -image "" -relief flat -bg gray70
label .r.f.f.pad -text "" -relief flat -bg gray70
label .r.f.addrlab -text "Addr/port/ttl: " -relief flat 
entry .r.f.f.addr  -bd 1 -textvariable session_address -relief flat -bg gray70
bind .r.f.f.addr <KeyPress-Return> { change_rtp_addr $session_address }

frame       .st -bd 0

checkbutton .st.file -image $icons(file_save) -indicatoron 0 -variable files_on  -command file_show
checkbutton .st.recp -bd 1 -bitmap reception -indicatoron 0 -variable matrix_on -command toggle_chart
checkbutton .st.help -bitmap balloon   -indicatoron 0 -variable help_on 

button      .st.opts  -text "Options..." -pady 2 -command {wm deiconify .prefs; update_user_panel; raise .prefs}
button      .st.about -text "About..."   -pady 2 -command {jiggle_credits; wm deiconify .about}
button      .st.quit  -text "Quit"       -pady 2 -command do_quit

frame .r.c -bd 0
frame .r.c.rx -relief groove -bd 2
frame .r.c.tx -relief groove -bd 2

frame .s -relief groove -bd 2 -width $iwd
label .s.status -text "Audio Status: " -relief flat -font $compfont
label .s.statustxt -text "wwwww" -relief flat -textvariable audio_device -font $compfont

pack .st -side bottom -fill x -padx 2 -pady 0
pack .st.file .st.recp .st.help -side left -anchor e -padx 2 -pady 2

pack .st.quit .st.about .st.opts -side right -anchor w -padx 2 -pady 2

pack .s -before .st -side bottom -fill x -padx 2 -pady 0
pack .s.status -side left -anchor e -padx 2 -pady 2
pack .s.statustxt -side left -anchor e -padx 2 -pady 2

# hide status bar as Default
pack forget .s

pack .r -side top -fill x -padx 2
pack .r.f -side top -fill x -padx 0 -pady 2
pack .r.f.addrlab .r.f.f -side left 
pack .r.f.f.addr .r.f.f.pad .r.f.f.padlock -side left -pady 2 -anchor n -fill x
pack .r.c -side top -fill x -expand 1
pack .r.c.rx -side left -fill x -expand 1
pack .r.c.tx -side left -fill x -expand 1

pack .l -side top -fill both -expand 1
#pack .l.f.title -side top -pady 2 -anchor w -fill x
pack .l.t  -side top -fill both -expand 1 -padx 2
if {$::tk_version < 8.5} {
    pack .l.t.scr -side left -fill y
} else {
    pack .l.t.scr -side right -fill y
}
pack .l.t.list -side left -fill both -expand 1
bind .l.t.list <Configure> {fix_scrollbar}

# Device output controls
set out_mute_var 0
frame .r.c.rx.net
frame .r.c.rx.au

pack .r.c.rx.net -side top -anchor n -fill both
pack .r.c.rx.au -side bottom -expand 1 -fill both

checkbutton .r.c.rx.net.on -highlightthickness 0 -text "Listen" -onvalue 0 -offvalue 1 -variable out_mute_var -command {output_mute $out_mute_var} -font $compfont -width 7 -anchor w -padx 4
label       .r.c.rx.net.bw -textv bps_in -font $compfont -anchor e -width 8
pack .r.c.rx.net.on -side left
pack .r.c.rx.net.bw -side right -fill x -expand 1

frame  .r.c.rx.au.port -bd 0
button .r.c.rx.au.port.l0 -highlightthickness 0 -command {incr_port oport oports -1} -font $compfont -bitmap right -relief flat
label  .r.c.rx.au.port.l1 -highlightthickness 0 -textv oport -font $compfont -width 10
button .r.c.rx.au.port.l2 -highlightthickness 0 -command {incr_port oport oports +1} -font $compfont -bitmap  left -relief flat
label  .r.c.rx.au.port.vlabel -text "Vol"
label  .r.c.rx.au.port.vval   -textv volume -width 3 -justify right
pack   .r.c.rx.au.port -side top -fill x -expand 1

pack   .r.c.rx.au.port.l2 -side left -fill y
pack   .r.c.rx.au.port.l1 -side left -fill x -expand 0
pack  .r.c.rx.au.port.l0 -side left -fill y


pack   .r.c.rx.au.port.vval .r.c.rx.au.port.vlabel -side right

frame  .r.c.rx.au.pow -relief sunk -bd 2
bargraph::create .r.c.rx.au.pow.bar
scale .r.c.rx.au.pow.sli -highlightthickness 0 -from 0 -to 99 -command set_vol -orient horizontal -showvalue false -width 8 -variable volume -resolution 4 -bd 0 -troughcolor black -sliderl 16
pack .r.c.rx.au.pow .r.c.rx.au.pow.bar .r.c.rx.au.pow.sli -side top -fill both -expand 1

# Device input controls
set in_mute_var 1

frame .r.c.tx.net
frame .r.c.tx.au

pack .r.c.tx.net -side top -anchor n  -fill both
pack .r.c.tx.au -side bottom -expand 1 -fill both

checkbutton .r.c.tx.net.on -highlightthickness 0 -text "Talk" -onvalue 0 -offvalue 1 -variable in_mute_var -command {input_mute $in_mute_var} -font $compfont -width 7 -anchor w -padx 4
label       .r.c.tx.net.bw -textv bps_out -font $compfont -anchor e -width 8
pack .r.c.tx.net.on -side left
pack .r.c.tx.net.bw -side right -fill x -expand 1

frame  .r.c.tx.au.port -bd 0
button .r.c.tx.au.port.l0 -highlightthickness 0 -command {incr_port iport iports -1} -font $compfont -bitmap right -relief flat
label  .r.c.tx.au.port.l1 -highlightthickness 0 -textv iport -font $compfont -width 10
button .r.c.tx.au.port.l2 -highlightthickness 0 -command {incr_port iport iports +1} -font $compfont -bitmap  left -relief flat
label  .r.c.tx.au.port.vlabel -text "Gain"
label  .r.c.tx.au.port.vval   -textv gain -width 3 -justify right
pack   .r.c.tx.au.port -side top -fill x -expand 1
pack   .r.c.tx.au.port.l2 -side left -fill y
pack   .r.c.tx.au.port.l1 -side left -fill x -expand 0
pack   .r.c.tx.au.port.l0 -side left -fill y
pack   .r.c.tx.au.port.vval .r.c.tx.au.port.vlabel -side right

frame  .r.c.tx.au.pow -relief sunk -bd 2
bargraph::create .r.c.tx.au.pow.bar
scale .r.c.tx.au.pow.sli -highlightthickness 0 -from 0 -to 99 -command set_gain -orient horizontal -showvalue false -width 8 -variable gain -resolution 4 -bd 0 -sliderl 16 -troughco black
pack .r.c.tx.au.pow .r.c.tx.au.pow.bar .r.c.tx.au.pow.sli -side top -fill both -expand 1

bind all <ButtonPress-3>   {toggle in_mute_var; input_mute $in_mute_var}
bind all <ButtonRelease-3> {toggle in_mute_var; input_mute $in_mute_var}
bind all <q>               {+if {[winfo class %W] != "Entry"} {do_quit}}

# Override default tk behaviour
wm protocol . WM_DELETE_WINDOW do_quit
wm title    . Initializing...

if {$win32 == 0} {
	wm iconbitmap . rat_small
}
#wm resizable . 0 1
if ([info exists geometry]) {
        wm geometry . $geometry
}

proc averageCharacterWidth {font} {
    set sample "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    set slen [string length $sample]
    set wpc  [expr [font measure $font $sample] / $slen + 1]
    return $wpc
}

# constrain_window - window, font, characters wide, characters high
proc constrain_window {w font cW cH} {

    catch {
            set wpc [averageCharacterWidth $font]
            set hpc [font metrics $font -ascent]

        # Calculate dimensions
            set width [expr $cW * $wpc]
            set height [expr $cH * $hpc]
            wm geometry $w [format "%sx%s" $width $height]
            set dummy ""
    } err
    if {$err != ""} {
        bgerror "Error: $err"
    }
}

proc tk_optionCmdMenu {w varName firstValue args} {
    upvar #0 $varName var

    if ![info exists var] {
        set var $firstValue
    }
    menubutton $w -textvariable $varName -indicatoron 1 -menu $w.menu \
            -relief raised -bd 2 -highlightthickness 2 -anchor c

    menu $w.menu -tearoff 0
    $w.menu add command -label $firstValue -command "set $varName \"$firstValue\""
    foreach i $args {
        $w.menu add command -label $i -command "set $varName \"$i\""
    }
    return $w.menu
}


###############################################################################
# Preferences Panel
#

set prefs_pane "Personal"
toplevel .prefs
wm title .prefs "Preferences"
wm resizable .prefs 0 0
wm withdraw  .prefs

frame .prefs.m
pack .prefs.m -side top -fill x -expand 0 -padx 2 -pady 2
frame .prefs.m.f
pack .prefs.m.f -padx 0 -pady 0
label .prefs.m.f.t -text "Category: "
pack .prefs.m.f.t -pady 2 -side left
menubutton .prefs.m.f.m -menu .prefs.m.f.m.menu -indicatoron 1 -textvariable prefs_pane -relief raised -width 14
pack .prefs.m.f.m -side top
menu .prefs.m.f.m.menu -tearoff 0
.prefs.m.f.m.menu add command -label "Personal"     -command {set_pane prefs_pane .prefs.pane "Personal"}
.prefs.m.f.m.menu add command -label "Transmission" -command {set_pane prefs_pane .prefs.pane "Transmission"; update_codecs_displayed}
.prefs.m.f.m.menu add command -label "Reception"    -command {set_pane prefs_pane .prefs.pane "Reception"}
.prefs.m.f.m.menu add command -label "Audio"        -command {set_pane prefs_pane .prefs.pane "Audio"}
.prefs.m.f.m.menu add command -label "Codecs"        -command {set_pane prefs_pane .prefs.pane "Codecs"; codecs_panel_fill}
.prefs.m.f.m.menu add command -label "Security"     -command {set_pane prefs_pane .prefs.pane "Security"}
.prefs.m.f.m.menu add command -label "Interface"    -command {set_pane prefs_pane .prefs.pane "Interface"}

frame  .prefs.buttons
pack   .prefs.buttons       -side bottom -fill x
button .prefs.buttons.bye   -text "Cancel" -command {sync_ui_to_engine; wm withdraw .prefs}
button .prefs.buttons.apply -text "Apply" -command {wm withdraw .prefs; sync_engine_to_ui}
pack   .prefs.buttons.bye .prefs.buttons.apply -side right -padx 2 -pady 2

wm protocol .prefs WM_DELETE_WINDOW {sync_ui_to_engine; wm withdraw .prefs}

frame .prefs.pane -relief groove -bd 2
pack  .prefs.pane -side left -fill both -expand 1 -padx 4 -pady 2

# setup width of prefs panel
constrain_window .prefs $infofont 56 30

# Personal Info Pane ##########################################################
set i .prefs.pane.personal
frame $i
pack $i -fill both -expand 1 -pady 2 -padx 2

frame $i.a
frame $i.a.f -rel fl
pack $i.a -side top -fill both -expand 1
pack $i.a.f -side left -fill x -expand 1

frame $i.a.f.f -rel fl
pack $i.a.f.f

label $i.a.f.f.l -width 40 -height 2 -text "The personal details below are conveyed\nto the other conference participants." -justify left -anchor w
pack $i.a.f.f.l -side top -anchor w -fill x

frame $i.a.f.f.lbls
frame $i.a.f.f.ents
pack  $i.a.f.f.lbls -side left -fill y
pack  $i.a.f.f.ents -side right

label $i.a.f.f.lbls.1 -text "Name:"     -anchor w
label $i.a.f.f.lbls.2 -text "Email:"    -anchor w
label $i.a.f.f.lbls.3 -text "Phone:"    -anchor w
label $i.a.f.f.lbls.4 -text "Location:" -anchor w
label $i.a.f.f.lbls.5 -text "Note:" -anchor w
pack $i.a.f.f.lbls.1 $i.a.f.f.lbls.2 $i.a.f.f.lbls.3 $i.a.f.f.lbls.4 $i.a.f.f.lbls.5 -fill x -anchor w -side top

entry $i.a.f.f.ents.name  -width 28 -highlightthickness 0 -textvariable rtcp_name
entry $i.a.f.f.ents.email -width 28 -highlightthickness 0 -textvariable rtcp_email
entry $i.a.f.f.ents.phone -width 28 -highlightthickness 0 -textvariable rtcp_phone
entry $i.a.f.f.ents.loc   -width 28 -highlightthickness 0 -textvariable rtcp_loc
entry $i.a.f.f.ents.note  -width 28 -highlightthickness 0 -textvariable rtcp_note
pack $i.a.f.f.ents.name $i.a.f.f.ents.email $i.a.f.f.ents.phone $i.a.f.f.ents.loc $i.a.f.f.ents.note -anchor n -expand 0

proc update_user_panel {} {
    global my_ssrc NAME EMAIL PHONE LOC NOTE PRIV
    global rtcp_name rtcp_email rtcp_phone rtcp_loc rtcp_note
    if {[info exists my_ssrc]} {
        set rtcp_name  $NAME($my_ssrc)
        set rtcp_email $EMAIL($my_ssrc)
        set rtcp_phone $PHONE($my_ssrc)
        set rtcp_loc   $LOC($my_ssrc)
        set rtcp_note  $NOTE($my_ssrc)
    }
}

# Transmission Pane ###########################################################
set i .prefs.pane.transmission
frame $i
frame $i.dd
frame $i.cc
frame $i.cc.van
frame $i.cc.red
frame $i.cc.layer
frame $i.cc.int
label $i.intro -text "This panel allows you to select codecs for transmission.  The choice\nof codecs available depends on the sampling rate and channels\nin the audio panel."
frame $i.title1-f -relief raised
label $i.title1-f.l -text "Audio Encoding"
pack $i.intro $i.title1-f $i.title1-f.l $i.dd -side top -fill x

#pack $i.dd -fill x -side top -anchor n

frame $i.title2-f -relief raised
label $i.title2-f.l -text "Channel Coding Options"
pack $i.title2-f $i.title2-f.l -fill x -side top
pack $i.cc -fill x -anchor w -pady 1

pack $i.cc.van $i.cc.red $i.cc.layer -fill x -anchor w -pady 0
# interleaving panel $i.cc.int not packed since interleaving isn't support in this release
frame $i.dd.units
frame $i.dd.pri

pack $i.dd.units $i.dd.pri -side right -fill x

label $i.dd.pri.l -text "Encoding:"
menubutton $i.dd.pri.m -menu $i.dd.pri.m.menu -indicatoron 1 -textvariable prenc -relief raised -width 13
pack $i.dd.pri.l $i.dd.pri.m -side top
# fill in codecs
menu $i.dd.pri.m.menu -tearoff 0

label $i.dd.units.l -text "Units:"
tk_optionCmdMenu $i.dd.units.m upp 1 2 4 8 16 32 64
$i.dd.units.m configure -width 13 -highlightthickness 0 -bd 1
pack $i.dd.units.l $i.dd.units.m -side top -fill x

radiobutton $i.cc.van.rb -text "No Loss Protection" -justify right -value none        -variable channel_var
radiobutton $i.cc.red.rb -text "Redundancy"         -justify right -value redundancy  -variable channel_var
radiobutton $i.cc.layer.rb -text "Layering"			-justify right -value layering    -variable channel_var
radiobutton $i.cc.int.rb -text "Interleaving"       -justify right -value interleaved -variable channel_var -state disabled
pack $i.cc.van.rb $i.cc.red.rb $i.cc.layer.rb $i.cc.int.rb -side left -anchor nw -padx 2

frame $i.cc.red.fc
label $i.cc.red.fc.l -text "Encoding:"
menubutton $i.cc.red.fc.m -textvariable secenc -indicatoron 1 -menu $i.cc.red.fc.m.menu -relief raised -width 13
menu $i.cc.red.fc.m.menu -tearoff 0

frame $i.cc.red.u
label $i.cc.red.u.l -text "Offset in Pkts:"
tk_optionCmdMenu $i.cc.red.u.m red_off "1" "2" "4" "8"
$i.cc.red.u.m configure -width 13 -highlightthickness 0 -bd 1
pack $i.cc.red.u -side right -anchor e -fill y
pack $i.cc.red.u.l $i.cc.red.u.m -fill x
pack $i.cc.red.fc -side right
pack $i.cc.red.fc.l $i.cc.red.fc.m

frame $i.cc.layer.fc
label $i.cc.layer.fc.l -text "Layers:"
menubutton $i.cc.layer.fc.m -textvariable layerenc -indicatoron 1 -menu $i.cc.layer.fc.m.menu -relief raised -width 13
menu $i.cc.layer.fc.m.menu -tearoff 0
pack $i.cc.layer.fc -side right
pack $i.cc.layer.fc.l $i.cc.layer.fc.m

frame $i.cc.int.zz
label $i.cc.int.zz.l -text "Units:"
tk_optionCmdMenu $i.cc.int.zz.m int_units 2 4 6 8
$i.cc.int.zz.m configure -width 13 -highlightthickness 0 -bd 1 -state disabled

frame $i.cc.int.fc
label $i.cc.int.fc.l -text "Separation:"
tk_optionCmdMenu $i.cc.int.fc.m int_gap 2 4 6 8
$i.cc.int.fc.m configure -width 13 -highlightthickness 0 -bd 1 -state disabled

pack $i.cc.int.fc $i.cc.int.zz -side right
pack $i.cc.int.fc.l $i.cc.int.fc.m -fill x -expand 1
pack $i.cc.int.zz.l $i.cc.int.zz.m -fill x -expand 1

# Reception Pane ##############################################################
set i .prefs.pane.reception
frame $i
frame $i.r -rel f
frame $i.o -rel f
frame $i.c -rel f
pack $i.r -side top -fill x -pady 0 -ipady 1
pack $i.o -side top -fill both  -pady 1
pack $i.c -side top -fill both  -pady 1 -expand 1
label $i.r.l -text "Repair Scheme:"
tk_optionCmdMenu $i.r.m repair_var {}

label $i.r.ls -text "Sample Rate Conversion"
tk_optionCmdMenu $i.r.ms convert_var {}

$i.r.m  configure -width 20 -bd 1
$i.r.ms configure -width 20 -bd 1
pack $i.r.l $i.r.m $i.r.ls $i.r.ms -side top

frame $i.o.f
checkbutton $i.o.f.cb -text "Limit Playout Delay" -variable limit_var
frame $i.o.f.fl
label $i.o.f.fl.l1 -text "Minimum Delay (ms)"
scale $i.o.f.fl.scmin -orient horizontal -from 0 -to 1000    -variable min_var -font $smallfont
frame $i.o.f.fr
label $i.o.f.fr.l2 -text "Maximum Delay (ms)"
scale $i.o.f.fr.scmax -orient horizontal -from 1000 -to 2000 -variable max_var -font $smallfont
pack $i.o.f
pack $i.o.f.cb -side top -fill x
pack $i.o.f.fl $i.o.f.fr -side left
pack $i.o.f.fl.l1 $i.o.f.fl.scmin $i.o.f.fr.l2 $i.o.f.fr.scmax -side top -fill x -expand 1

frame $i.c.f
frame $i.c.f.f
checkbutton $i.c.f.f.lec -text "Lecture Mode"       -variable lecture_var
checkbutton $i.c.f.f.ext -text "3D Audio Rendering" -variable 3d_audio_var

pack $i.c.f -fill x -side left -expand 1
pack $i.c.f.f
pack $i.c.f.f.lec -side top  -anchor w
pack $i.c.f.f.ext -side top  -anchor w

# Audio #######################################################################
set i .prefs.pane.audio
frame $i
frame $i.dd -rel fl
pack $i.dd -fill both -expand 1 -anchor w -pady 1

label $i.dd.title -height 2 -width 40 -text "This panel allows for the selection of alternate audio devices\nand the configuring of device related options." -justify left
pack $i.dd.title -fill x

frame $i.dd.device
pack $i.dd.device -side top

label $i.dd.device.l -text "Audio Device:"
pack  $i.dd.device.l -side top -fill x
menubutton $i.dd.device.mdev -menu $i.dd.device.mdev.menu -indicatoron 1 \
                                -textvariable audio_device -relief raised -width 5
pack $i.dd.device.mdev -fill x -expand 1
menu $i.dd.device.mdev.menu -tearoff 0

frame $i.dd.sampling
pack  $i.dd.sampling

frame $i.dd.sampling.freq
frame $i.dd.sampling.ch_in
pack $i.dd.sampling.freq $i.dd.sampling.ch_in -side left -fill x

label $i.dd.sampling.freq.l   -text "Sample Rate:   "
label $i.dd.sampling.ch_in.l  -text "Channels:"
pack $i.dd.sampling.freq.l $i.dd.sampling.ch_in.l -fill x

menubutton $i.dd.sampling.freq.mb -menu $i.dd.sampling.freq.mb.m -indicatoron 1 \
                                  -textvariable freq -relief raised
pack $i.dd.sampling.freq.mb -side left -fill x -expand 1
menu $i.dd.sampling.freq.mb.m

menubutton $i.dd.sampling.ch_in.mb -menu $i.dd.sampling.ch_in.mb.m -indicatoron 1 \
                                  -textvariable ichannels -relief raised
pack $i.dd.sampling.ch_in.mb -side left -fill x -expand 1
menu $i.dd.sampling.ch_in.mb.m

frame $i.dd.cks
pack $i.dd.cks -fill both -expand 1
frame $i.dd.cks.f
frame $i.dd.cks.f.f
frame $i.dd.cks.f.f.silence
frame $i.dd.cks.f.f.other

frame $i.dd.cks.f.f.silence.upper
label $i.dd.cks.f.f.silence.upper.title    -text "Silence Suppression:"
radiobutton $i.dd.cks.f.f.silence.upper.r0 -text "Off"       -value "Off" -variable silence_var       -command send_silence_params
radiobutton $i.dd.cks.f.f.silence.upper.r1 -text "Automatic" -value "Automatic" -variable silence_var -command send_silence_params
radiobutton $i.dd.cks.f.f.silence.upper.r2 -text "Manual"    -value "Manual" -variable silence_var    -command send_silence_params

frame $i.dd.cks.f.f.silence.lower
scale $i.dd.cks.f.f.silence.lower.s -from 1 -to 500 -orient h -showvalue 0 -variable manual_silence_thresh -command send_silence_params
label $i.dd.cks.f.f.silence.lower.ind -textvar manual_silence_thresh -width 5
label $i.dd.cks.f.f.silence.lower.title -text "Manual Silence Threshold:"
pack $i.dd.cks.f.f.silence.lower.title  -side top
pack $i.dd.cks.f.f.silence.lower.s $i.dd.cks.f.f.silence.lower.ind -side left

pack $i.dd.cks.f.f.silence.upper.title -side top -fill x
pack $i.dd.cks.f.f.silence.upper.r0 $i.dd.cks.f.f.silence.upper.r1 $i.dd.cks.f.f.silence.upper.r2 -side left

label $i.dd.cks.f.f.other.title -text "Additional Audio Options:"
checkbutton $i.dd.cks.f.f.other.agc      -text "Automatic Gain Control" -variable agc_var
checkbutton $i.dd.cks.f.f.other.loop     -text "Audio Loopback"         -variable audio_loop_var
checkbutton $i.dd.cks.f.f.other.suppress -text "Echo Suppression"       -variable echo_var
checkbutton $i.dd.cks.f.f.other.tone     -text "Tone Test"              -variable tonegen        -command send_tone_cmd
pack $i.dd.cks.f -fill x -side top -expand 1
pack $i.dd.cks.f.f
pack $i.dd.cks.f.f.silence -side left -anchor n
pack $i.dd.cks.f.f.silence.upper -side top -anchor n
pack $i.dd.cks.f.f.silence.lower -side bottom -anchor n
pack $i.dd.cks.f.f.other   -side right -anchor n
pack $i.dd.cks.f.f.other.title -side top
pack $i.dd.cks.f.f.other.agc $i.dd.cks.f.f.other.loop $i.dd.cks.f.f.other.suppress $i.dd.cks.f.f.other.tone -side top -anchor w

# Codecs pane #################################################################
set i .prefs.pane.codecs
frame $i

frame $i.of -rel fl
pack  $i.of -fill both -expand 1 -anchor w -pady 1 -side top

message $i.of.l -width 30c -justify left -text "This panel shows available codecs and allows RTP payload re-mapping."
pack $i.of.l -side top -fill x

frame   $i.of.codecs

pack    $i.of.codecs -side left -padx 2 -fill y
frame   $i.of.codecs.l -rel raised
label   $i.of.codecs.l.l -text "Codec"
listbox $i.of.codecs.lb -width 20 -yscrollcommand "$i.of.codecs.scroll set" -relief fl -bg white
if {$::tk_version < 8.5} {
    scrollbar $i.of.codecs.scroll -command "$i.of.codecs.lb yview" -rel fl

    pack    $i.of.codecs.l $i.of.codecs.l.l -side top -fill x
    pack    $i.of.codecs.scroll $i.of.codecs.lb -side left -fill both
} else {
    ttk::scrollbar $i.of.codecs.scroll -command "$i.of.codecs.lb yview"

    pack    $i.of.codecs.l $i.of.codecs.l.l -side top -fill x
    pack    $i.of.codecs.scroll $i.of.codecs.lb -side right -fill both
}

frame   $i.of.details -bd 0
pack    $i.of.details -side left -fill both -expand 1 -anchor n

frame $i.of.details.upper
pack $i.of.details.upper -fill x

frame $i.of.details.desc
pack $i.of.details.desc -side top -fill x

frame $i.of.details.pt
pack $i.of.details.pt -side bottom -fill x -anchor s
label $i.of.details.pt.l -anchor w -text "RTP payload:"
pack  $i.of.details.pt.l -side left -anchor w

entry $i.of.details.pt.e -width 4
pack  $i.of.details.pt.e -side left -padx 4

button $i.of.details.pt.b -text "Map Codec" -command map_codec
pack  $i.of.details.pt.b -side right -padx 4

frame $i.of.details.upper.l0 -relief raised
label $i.of.details.upper.l0.l -text "Details"
pack $i.of.details.upper.l0 $i.of.details.upper.l0.l -side top -fill x -expand 1

frame $i.of.details.upper.l
pack $i.of.details.upper.l -side left
label $i.of.details.upper.l.0 -text "Short name:"  -anchor w
label $i.of.details.upper.l.1 -text "Sample Rate (Hz):" -anchor w
label $i.of.details.upper.l.2 -text "Channels:"    -anchor w
label $i.of.details.upper.l.3 -text "Bitrate (kbps):"     -anchor w
label $i.of.details.upper.l.4 -text "RTP Payload:" -anchor w
label $i.of.details.upper.l.5 -text "Capability:" -anchor w
label $i.of.details.upper.l.6 -text "Layers:" -anchor w

for {set idx 0} {$idx < 7} {incr idx} {
    pack $i.of.details.upper.l.$idx -side top -fill x
}

frame $i.of.details.upper.r
pack $i.of.details.upper.r -side left -fill x -expand 1
label $i.of.details.upper.r.0 -anchor w
label $i.of.details.upper.r.1 -anchor w
label $i.of.details.upper.r.2 -anchor w
label $i.of.details.upper.r.3 -anchor w
label $i.of.details.upper.r.4 -anchor w
label $i.of.details.upper.r.5 -anchor w
label $i.of.details.upper.r.6 -anchor w

for {set idx 0} {$idx < 7} {incr idx} {
    pack $i.of.details.upper.r.$idx -side top -fill x
}

set descw [expr [averageCharacterWidth $infofont] * 30]
label $i.of.details.desc.l -text "Description:" -anchor w -wraplength $descw -justify left
pack $i.of.details.desc.l -side left -fill x
unset descw

bind $i.of.codecs.lb <1> {
    codecs_panel_select [%W index @%x,%y]
}

bind $i.of.codecs.lb <ButtonRelease-1> {
    codecs_panel_select [%W index @%x,%y]
}
proc codecs_panel_fill {} {
    global codecs

    .prefs.pane.codecs.of.codecs.lb delete 0 end

    foreach {c} $codecs {
	.prefs.pane.codecs.of.codecs.lb insert end $c
    }
}

set last_selected_codec -1

proc codecs_panel_select { idx } {
    global codecs codec_nick_name codec_rate codec_channels codec_pt codec_block_size codec_data_size codec_desc codec_caps codec_layers
    global last_selected_codec

    set last_selected_codec $idx

    set codec [lindex $codecs $idx]
    set root  .prefs.pane.codecs.of.details.upper.r
    $root.0 configure -text $codec_nick_name($codec)
    $root.1 configure -text $codec_rate($codec)
    $root.2 configure -text $codec_channels($codec)

    set fps [expr $codec_rate($codec) * 2 * $codec_channels($codec) / $codec_block_size($codec) ]
    set kbps [expr 8 * $fps * $codec_data_size($codec) / 1000.0]
    $root.3 configure -text [format "%.1f" $kbps]

    $root.4 configure -text $codec_pt($codec)
    $root.5 configure -text $codec_caps($codec)
    $root.6 configure -text $codec_layers($codec)

    .prefs.pane.codecs.of.details.desc.l configure -text "Description: $codec_desc($codec)"

}

proc map_codec {} {
    global codecs last_selected_codec

    set idx $last_selected_codec

    if {$last_selected_codec == -1} {
	return
    }

    set pt [.prefs.pane.codecs.of.details.pt.e get]
    .prefs.pane.codecs.of.details.pt.e delete 0 end

    set ptnot [string trim $pt 1234567890]
    if {$ptnot != ""} {
	return
    }

    set codec [lindex $codecs $idx]

    mbus_send "R" "tool.rat.payload.set" "[mbus_encode_str $codec] $pt"
    after 1000 codecs_panel_select $idx
}

# Security Pane ###############################################################
set i .prefs.pane.security
frame $i
frame $i.a -rel fl
frame $i.a.f
frame $i.a.f.f
label $i.a.f.f.l -anchor w -justify left -text "Your communication can be secured with\nDES or AES encryption.  Only conference participants\nwith the same key can receive audio data when\nencryption is enabled."
pack $i.a.f.f.l
pack $i.a -side top -fill both -expand 1
label $i.a.f.f.lbl -text "Key:"
entry $i.a.f.f.e -width 28 -textvariable key
checkbutton $i.a.f.f.cb -text "Enabled" -variable key_var -command update_security_indicator
pack $i.a.f -fill x -side left -expand 1
pack $i.a.f.f
pack $i.a.f.f.lbl $i.a.f.f.e $i.a.f.f.cb -side left -pady 4 -padx 2 -fill x

# Interface Pane ##############################################################
set i .prefs.pane.interface
frame $i
frame $i.a -rel fl
frame $i.a.f
frame $i.a.f.f
label $i.a.f.f.l -anchor w -justify left -text "The following features may be\ndisabled to conserve processing\npower."
pack $i.a -side top -fill both -expand 1
pack $i.a.f -fill x -side left -expand 1
checkbutton $i.a.f.f.power   -text "Powermeters active"       -variable meter_var
checkbutton $i.a.f.f.balloon -text "Balloon help"             -variable help_on
checkbutton $i.a.f.f.matrix  -text "Reception quality matrix" -variable matrix_on -command toggle_chart
checkbutton $i.a.f.f.plist   -text "Participant list"         -variable plist_on  -command toggle_plist
checkbutton $i.a.f.f.fwin    -text "File Control Window"      -variable files_on  -command file_show
checkbutton $i.a.f.f.status  -text "Status bar"               -variable status_on  -command toggle_status
pack $i.a.f.f $i.a.f.f.l
pack $i.a.f.f.power $i.a.f.f.balloon $i.a.f.f.matrix $i.a.f.f.plist $i.a.f.f.fwin $i.a.f.f.status -side top -anchor w

proc set_pane {p base desc} {
    upvar 1 $p pane
    set tpane [string tolower [lindex [split $pane] 0]]
    pack forget $base.$tpane
    set tpane [string tolower [lindex [split $desc] 0]]
    pack $base.$tpane -fill both -expand 1 -padx 2 -pady 2
    set pane $desc
}

# Initialise "About..." toplevel window
toplevel   .about
frame      .about.rim -relief groove -bd 2
frame      .about.m
frame      .about.rim.d
pack .about.m -fill x
pack .about.rim -padx 4 -pady 2 -side top -fill both -expand 1
pack .about.rim.d -side top -fill both -expand 1

frame .about.m.f
label .about.m.f.l -text "Category:"
menubutton .about.m.f.mb -menu .about.m.f.mb.menu -indicatoron 1 -textvariable about_pane -relief raised -width 10
menu .about.m.f.mb.menu -tearoff 0
.about.m.f.mb.menu add command -label "Credits"   -command {set_pane about_pane .about.rim.d "Credits"   }
.about.m.f.mb.menu add command -label "Feedback"  -command {set_pane about_pane .about.rim.d "Feedback"  }
.about.m.f.mb.menu add command -label "Copyright" -command {set_pane about_pane .about.rim.d "Copyright" }

pack .about.m.f
pack .about.m.f.l .about.m.f.mb -side left

frame     .about.rim.d.copyright
frame     .about.rim.d.copyright.f -relief flat
frame     .about.rim.d.copyright.f.f
text      .about.rim.d.copyright.f.f.blurb -height 14 -yscrollcommand ".about.rim.d.copyright.f.f.scroll set" -relief flat -bg white
if {$::tk_version < 8.5} {
    scrollbar .about.rim.d.copyright.f.f.scroll -command ".about.rim.d.copyright.f.f.blurb yview" -relief flat
} else {
    ttk::scrollbar .about.rim.d.copyright.f.f.scroll -command ".about.rim.d.copyright.f.f.blurb yview"
}
pack      .about.rim.d.copyright.f -expand 1 -fill both
pack      .about.rim.d.copyright.f.f -expand 1 -fill both
if {$::tk_version < 8.5} {
    pack  .about.rim.d.copyright.f.f.scroll -side left -fill y -expand 1
} else {
    pack  .about.rim.d.copyright.f.f.scroll -side right -fill y -expand 1
}
pack      .about.rim.d.copyright.f.f.blurb -side left -fill y -expand 1

frame     .about.rim.d.credits
frame     .about.rim.d.credits.f -relief flat
frame     .about.rim.d.credits.f.f
pack      .about.rim.d.credits.f -fill both -expand 1
pack      .about.rim.d.credits.f.f -side left -fill x -expand 1
label     .about.rim.d.credits.f.f.1                  -text "The Robust-Audio Tool was developed in the Department of\nComputer Science, University College London.\n\nProject Supervision:"
label     .about.rim.d.credits.f.f.2 -foreground blue -text Good
label     .about.rim.d.credits.f.f.3                  -text "Development Team:"
label     .about.rim.d.credits.f.f.4 -foreground blue -text Bad
label     .about.rim.d.credits.f.f.5                  -text "Additional Contributions:"
label     .about.rim.d.credits.f.f.6 -foreground blue -text Ugly
for {set i 1} {$i<=6} {incr i} {
    pack  .about.rim.d.credits.f.f.$i -side top -fill x -expand 0 -anchor n
}

frame     .about.rim.d.feedback
frame     .about.rim.d.feedback.f -relief flat
frame     .about.rim.d.feedback.f.f
pack      .about.rim.d.feedback.f -fill both -expand 1
pack      .about.rim.d.feedback.f.f -side left -fill x -expand 1
label     .about.rim.d.feedback.f.f.1                  -text "A mailing list exists for general comments and discussion:"
label     .about.rim.d.feedback.f.f.2 -foreground blue -text "rat-users@cs.ucl.ac.uk"
label     .about.rim.d.feedback.f.f.3                  -text "This list is open to all, you are encouraged to subscribe"
label     .about.rim.d.feedback.f.f.4                  -text "(send mail to rat-users-request@cs.ucl.ac.uk to join).\n"
label     .about.rim.d.feedback.f.f.5                  -text "To reach the tool developers, send email to:"
label     .about.rim.d.feedback.f.f.6 -foreground blue -text "rat-trap@cs.ucl.ac.uk\n"
label     .about.rim.d.feedback.f.f.7                  -text "Further information is available on the world-wide web at:"
label     .about.rim.d.feedback.f.f.8 -foreground blue -text "http://www-mice.cs.ucl.ac.uk/multimedia/software/rat/\n"
for {set i 1} {$i<=8} {incr i} {
    pack  .about.rim.d.feedback.f.f.$i -side top -fill x
}

wm withdraw  .about
wm title     .about "About RAT"
wm resizable .about 0 0
wm protocol  .about WM_DELETE_WINDOW {wm withdraw .about}

set about_pane Copyright
set_pane about_pane .about.rim.d "Credits"
constrain_window .about $infofont 64 25

.about.rim.d.copyright.f.f.blurb insert end \
{Copyright (C) 1995-2006 University College London
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

Copyright (C) 1991-1993,1996 Regents of the University of California
Copyright (C) 1992 Stichting Mathematisch Centrum, Amsterdam
Copyright (C) 1991-1992 RSA Data Security, Inc
Copyright (C) 1992 Jutta Degener and Carsten Bormann, TU Berlin
Copyright (C) 1994 Paul Stewart
Copyright (C) 2000 Nortel Networks
Copyright (C) 2000 Argonne National Laboratory
Copyright (C) 1991 Bell Communications Research, Inc. (Bellcore)
Copyright (C) 1996 Internet Software Consortium.
Copyright (C) 1995-1999 WIDE Project

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

proc shuffle_rats {args} {
    # This should really animate the movement and play fruit-machine sounds.... :-)
    set r ""
    set end [llength $args]
    set l 1
    while { $l <= $end } {
		set toget [expr abs([clock clicks]) % [llength $args]]
		set r [format "%s%s  " $r [lindex $args $toget]]
		set args [lreplace $args $toget $toget]
		if {$l != $end && [expr $l % 3] == 0} {
			set r "$r\n"
		}
		incr l
    }
    return $r
}

proc jiggle_credits {} {
# Software really developed by the Socialist Department of Computer Science
    .about.rim.d.credits.f.f.2 configure -text [shuffle_rats "Angela Sasse" "Vicky Hardman" "Peter Kirstein"]
    .about.rim.d.credits.f.f.4 configure -text [shuffle_rats "Colin Perkins" "Orion Hodson"]
    .about.rim.d.credits.f.f.6 configure -text [shuffle_rats "Isidor Kouvelas" "Darren Harris" "Anna Watson" "Mark Handley" "Jon Crowcroft" "Marcus Iken" "Kris Hasler" "Tristan Henderson" "Dimitrios Miras" "Socrates Varakliotis" "Piers O'Hanlon"]
}

proc sync_ui_to_engine {} {
    # the next time the display is shown, it needs to reflect the
    # state of the audio engine.
    mbus_send "R" "tool.rat.settings" ""
}


proc sync_engine_to_ui {} {
    # make audio engine concur with ui - called when "Apply" button clicked
    global my_ssrc rtcp_name rtcp_email rtcp_phone rtcp_loc rtcp_note
    global prenc upp channel_var secenc layerenc red_off int_gap int_units
    global agc_var audio_loop_var echo_var
    global repair_var limit_var min_var max_var lecture_var 3d_audio_var convert_var
    global meter_var gain volume iport oport
    global in_mute_var out_mute_var ichannels freq key key_var
    global audio_device received

    #rtcp details
    mbus_send "R" "rtp.source.name"  "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_name]"
    mbus_send "R" "rtp.source.email" "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_email]"
    mbus_send "R" "rtp.source.phone" "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_phone]"
    mbus_send "R" "rtp.source.loc"   "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_loc]"
    mbus_send "R" "rtp.source.note"  "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_note]"

    #cheat to update my details right away (it's disgusting)
    mbus_recv_rtp.source.name  $my_ssrc "$rtcp_name"
    mbus_recv_rtp.source.email $my_ssrc "$rtcp_email"
    mbus_recv_rtp.source.phone $my_ssrc "$rtcp_phone"
    mbus_recv_rtp.source.loc   $my_ssrc "$rtcp_loc"
    mbus_recv_rtp.source.note  $my_ssrc "$rtcp_note"

    #transmission details
    mbus_send "R" "tool.rat.codec"      "[mbus_encode_str $prenc] [mbus_encode_str $ichannels] [mbus_encode_str $freq]"
    mbus_send "R" "tool.rat.rate"         $upp

    switch $channel_var {
    	none         {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var]"}
	redundancy   {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] [mbus_encode_str $secenc] $red_off"}
	interleaved {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] $int_gap $int_units"}
	layering	{mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] [mbus_encode_str $prenc] [mbus_encode_str $ichannels] [mbus_encode_str $freq] $layerenc"}
    	*           {error "unknown channel coding scheme $channel_var"}
    }

    mbus_send "R" "tool.rat.agc"               $agc_var
    mbus_send "R" "tool.rat.loopback.gain"     $audio_loop_var
    mbus_send "R" "tool.rat.echo.suppress"     $echo_var

    #Reception Options
    mbus_send "R" "audio.channel.repair"   [mbus_encode_str $repair_var]
    mbus_send "R" "tool.rat.playout.limit" $limit_var
    mbus_send "R" "tool.rat.playout.min"   $min_var
    mbus_send "R" "tool.rat.playout.max"   $max_var
    mbus_send "R" "tool.rat.lecture.mode"  $lecture_var
    mbus_send "R" "audio.3d.enabled"    $3d_audio_var
    mbus_send "R" "tool.rat.converter"     [mbus_encode_str $convert_var]

    #Security
    if {$key_var==1 && [string length $key]!=0} {
	mbus_send "R" "security.encryption.key" [mbus_encode_str $key]
	update_security_indicator 
    } else {
	mbus_send "R" "security.encryption.key" [mbus_encode_str ""]
	update_security_indicator 
    }

    #Interface
    mbus_send "R" "tool.rat.powermeter"   $meter_var

    #device
    mbus_send "R" "audio.device"        [mbus_encode_str "$audio_device"]
    if {$received(gain)} {
	mbus_send "R" "audio.input.gain"    $gain
    }
    if {$received(volume)} {
	mbus_send "R" "audio.output.gain"   $volume
    }
    mbus_send "R" "audio.input.port"    [mbus_encode_str $iport]
    mbus_send "R" "audio.output.port"   [mbus_encode_str $oport]
    mbus_send "R" "audio.input.mute"    $in_mute_var
    mbus_send "R" "audio.output.mute"   $out_mute_var

    send_silence_params
}

proc send_silence_params {args} {
# These get special treatment and are sent as soon as they are changed
# because if user is setting a threshold they need to tell how it works
# immediately
    global silence_var manual_silence_thresh
    mbus_send "R" "tool.rat.silence"           [mbus_encode_str $silence_var]
    mbus_send "R" "tool.rat.silence.threshold" $manual_silence_thresh
}

#
# Routines to display the "chart" of RTCP RR statistics...
#

proc mtrace_callback {fd src dst} {
	if [winfo exists .mtrace-$src-$dst] {
		.mtrace-$src-$dst.t insert end [read $fd]
	} else {
		# The user has closed the mtrace window before the trace has completed
		close $fd
	}
	if [eof $fd] {
		close $fd
	}
}

proc mtrace {src dst} {
	global CNAME group_addr
	regsub {.*@([0-9]+.[0-9]+.[0-9]+.[0-9]+)} $CNAME($src) {\1} src_addr
	regsub {.*@([0-9]+.[0-9]+.[0-9]+.[0-9]+)} $CNAME($dst) {\1} dst_addr

	toplevel  .mtrace-$src-$dst
	text      .mtrace-$src-$dst.t -background white -font "Courier 8" -wrap none \
	                              -yscrollcommand ".mtrace-$src-$dst.sr set" \
	                              -xscrollcommand ".mtrace-$src-$dst.sb set"
	if {$::tk_version < 8.5} {
		scrollbar .mtrace-$src-$dst.sb -command ".mtrace-$src-$dst.t xview" -orient horizontal
		scrollbar .mtrace-$src-$dst.sr -command ".mtrace-$src-$dst.t yview" -orient vertical
	} else {
		ttk::scrollbar .mtrace-$src-$dst.sb -command ".mtrace-$src-$dst.t xview" -orient horizontal
		ttk::scrollbar .mtrace-$src-$dst.sr -command ".mtrace-$src-$dst.t yview" -orient vertical
	}
	pack .mtrace-$src-$dst.sb -fill x    -expand 0 -side bottom -anchor s
	pack .mtrace-$src-$dst.sr -fill y    -expand 0 -side right  -anchor e
	pack .mtrace-$src-$dst.t  -fill both -expand 1 -side left   -anchor w

	wm title .mtrace-$src-$dst "mtrace from $src_addr to $dst_addr via group $group_addr"

	if [file executable /usr/sbin/mtrace] {
		set mtrace_prog /usr/sbin/mtrace
	} else {
		set mtrace_prog mtrace
	}
	set fd [open "|$mtrace_prog $src_addr $dst_addr $group_addr" "r"]
	fconfigure $fd -blocking 0
	fileevent  $fd readable "mtrace_callback $fd $src $dst"
}

chart::create

#
# End of RTCP RR chart routines
#

#
# File Control Window
#

set play_file(state) end
set rec_file(state) end

catch {
    toplevel .file
    frame .file.play -relief groove -bd 2
    frame .file.rec  -relief groove -bd 2
    pack  .file.play -side top -pady 2 -padx 2 -fill x -expand 1
    pack  .file.rec  -side top -pady 2 -padx 2 -fill x -expand 1

    label .file.play.l -text "Playback"
    pack  .file.play.l -side top -fill x
    label .file.rec.l -text "Record"
    pack  .file.rec.l -side top -fill x

    wm withdraw .file
    wm title	.file "RAT File Control"
    wm protocol .file WM_DELETE_WINDOW "set files_on 0; file_show"

    foreach action { play rec } {
	frame  .file.$action.buttons
	pack   .file.$action.buttons
	button .file.$action.buttons.disk -bitmap disk -command "fileDialog $action"
	pack   .file.$action.buttons.disk -side left -padx 2 -pady 2 -anchor n

	foreach cmd "$action pause stop" {
	    button .file.$action.buttons.$cmd -bitmap $cmd -state disabled -command file_$action\_$cmd
	    pack   .file.$action.buttons.$cmd -side left -padx 2 -pady 2 -anchor n -fill x
	}

	label  .file.$action.buttons.status -text "No file selected." -width 16 -anchor w
	pack   .file.$action.buttons.status -side bottom -fill both -expand 1 -padx 2 -pady 2
    }
} fwinerr

if {$fwinerr != {}} {
	bgerror "$fwinerr"
}

proc fileDialog {cmdbox} {
    global win32 tcl_platform

	set defaultExtension wav
	set defaultLocation  [file nativename "~/"]

    set types {
		{"NeXT/Sun Audio files"	"au"}
		{"Microsoft RIFF files"	"wav"}
		{"All files"		"*"}
    }

    if {![string compare $cmdbox "play"]} {
		catch { asFileBox .playfilebox  -defaultextension $defaultExtension -command file_open_$cmdbox -directory $defaultLocation -extensions $types } asferror
    } else {
		catch { asFileBox .recfilebox   -defaultextension $defaultExtension -command file_open_$cmdbox  -extensions $types -force_extension 1 } asferror
    }

	if {$asferror != ""} {
		bgerror "$asferror"
	}
}

proc file_show {} {
    global files_on

    if {$files_on} {
	 	wm deiconify .file
    } else {
 		wm withdraw .file
    }
}

proc file_play_live {} {
# Request heart beat to determine if file is valid
	mbus_send "R" audio.file.play.live ""
}

proc file_rec_live {} {
# Request heart beat to determine if file is valid
	mbus_send "R" audio.file.record.live ""
}

proc file_open_play {path} {
    global play_file

    mbus_send "R" "audio.file.play.open" [mbus_encode_str $path]
    mbus_send "R" "audio.file.play.pause" 1
    set play_file(state) paused
    set play_file(name) $path

    # Test whether file is still playing/valid
    after 200 file_play_live
}

proc file_open_rec {path} {
    global rec_file

    mbus_send "R" "audio.file.record.open" [mbus_encode_str $path]
    mbus_send "R" "audio.file.record.pause" 1

    set rec_file(state) paused
    set rec_file(name)  $path

    # Test whether file is still recording/valid
    after 200 file_rec_live
}

proc file_enable_play { } {
    .file.play.buttons.play   configure -state normal
    .file.play.buttons.pause  configure -state disabled
    .file.play.buttons.stop   configure -state disabled
    .file.play.buttons.status configure -text "Ready to play."
}

proc file_enable_record { } {
    .file.rec.buttons.rec configure -state normal
    .file.rec.buttons.pause  configure -state disabled
    .file.rec.buttons.stop   configure -state disabled
    .file.rec.buttons.status configure -text "Ready to record."
}

proc file_play_play {} {
	global play_file

	catch {
		if {$play_file(state) == "paused"} {
			mbus_send "R" "audio.file.play.pause" 0
		} else {
			mbus_send "R" "audio.file.play.open" [mbus_encode_str $play_file(name)]
		}
		set play_file(state) play
	} pferr

	if { $pferr != "play" } { bgerror "pferr: $pferr" }

	.file.play.buttons.play   configure -state disabled
	.file.play.buttons.pause  configure -state normal
	.file.play.buttons.stop   configure -state normal
	.file.play.buttons.status configure -text "Playing."
	after 200 file_play_live
}

proc file_play_pause {} {
    global play_file

    .file.play.buttons.play   configure -state normal
    .file.play.buttons.pause  configure -state disabled
    .file.play.buttons.stop   configure -state normal
    .file.play.buttons.status configure -text "Paused."
    set play_file(state) paused
    mbus_send "R" "audio.file.play.pause" 1
}

proc file_play_stop {} {
	global play_file

	set play_file(state) end
	file_enable_play
	mbus_send "R" "audio.file.play.stop" ""
}

proc file_rec_rec {} {
	global rec_file

	catch {
		if {$rec_file(state) == "paused"} {
			mbus_send "R" "audio.file.record.pause" 0
		} else {
			mbus_send "R" "audio.file.record.open" [mbus_encode_str $rec_file(name)]
		}
		set rec_file(state) record
	} prerr

	if { $prerr != "record" } { bgerror "prerr: $prerr" }

	.file.rec.buttons.rec    configure -state disabled
	.file.rec.buttons.pause  configure -state normal
	.file.rec.buttons.stop   configure -state normal
	.file.rec.buttons.status configure -text "Recording."
	after 200 file_rec_live
}

proc file_rec_pause {} {
    global rec_file

    .file.rec.buttons.rec    configure -state normal
    .file.rec.buttons.pause  configure -state disabled
    .file.rec.buttons.stop   configure -state normal
    .file.rec.buttons.status configure -text "Paused."
    set rec_file(state) paused
    mbus_send "R" "audio.file.record.pause" 1
}

proc file_rec_stop {} {
	global rec_file

	set rec_file(state) end
	file_enable_record
	mbus_send "R" "audio.file.record.stop" ""
}

#
# End of File Window routines
#

help::add .r.c.tx.au.pow.sli 	"This slider controls the volume\nof the sound you send." "tx_gain.au"
help::add .r.c.tx.au.port.l0 	"Click to change input device." "tx_port.au"
help::add .r.c.tx.au.port.l1  	"Shows currently selected input device.\nClick the arrows on either side to change." "rx_port_name.au"
help::add .r.c.tx.au.port.l2 	"Click to change input device." "tx_port.au"
help::add .r.c.tx.net.on 	"If this button is pushed in, you are are transmitting, and\nmay be\
                                 heard by other participants. Holding down the\nright mouse button in\
                                 any RAT window will temporarily\ntoggle the state of this button,\
                                 allowing for easy\npush-to-talk operation." "tx_mute.au"
help::add .r.c.tx.au.pow.bar 	"Indicates the loudness of the\nsound you are sending. If this\nis\
                                 moving, you may be heard by\nthe other participants." "tx_powermeter.au"

help::add .r.c.rx.au.pow.sli  	"This slider controls the volume\nof the sound you hear." "rx_gain.au"
help::add .r.c.rx.au.port.l0  	"Click to change output device." "rx_port.au"
help::add .r.c.rx.au.port.l1  	"Shows currently selected output device.\nClick the arrows on either side to change." "rx_port_name.au"
help::add .r.c.rx.au.port.l2  	"Click to change output device." "rx_port.au"
help::add .r.c.rx.net.on  	"If pushed in, reception is muted." "rx_mute.au"
help::add .r.c.rx.au.pow.bar  	"Indicates the loudness of the\nsound you are hearing." "rx_powermeter.au"

help::add .r.f		"Name of the session, and the IP address, port\n&\
		 	 TTL used to transmit the audio data." "session_title.au"
help::add .l.t		"The participants in this session with you at the top.\nClick on a name\
                         with the left mouse button to display\ninformation on that participant,\
			 and with the middle\nbutton to mute that participant (the right button\nwill\
			 toggle the transmission mute button, as usual)." "participant_list.au"

help::add .st.recp 	"Displays a chart showing the reception\nquality reported by all participants" "rqm_enable.au"
help::add .st.help       "If you can see this, balloon help\nis enabled. If not, it isn't." "balloon_help.au"
help::add .st.opts   	"Brings up another window allowing\nthe control of various options." "options.au"
help::add .st.about  	"Brings up another window displaying\ncopyright & author information." "about.au"
help::add .st.quit   	"Press to leave the session." "quit.au"

# preferences help
help::add .prefs.m.f.m  "Click here to change the preference\ncategory." "prefs_category.au"
set i .prefs.buttons
help::add $i.bye         "Cancel changes." "cancel_changes.au"
help::add $i.apply       "Apply changes." "apply_changes.au"

# user help
set i .prefs.pane.personal.a.f.f.ents
help::add $i.name      	"Enter your name for transmission\nto other participants." "user_name.au"
help::add $i.email      	"Enter your email address for transmission\nto other participants." "user_email.au"
help::add $i.phone     	"Enter your phone number for transmission\nto other participants." "user_phone.au"
help::add $i.loc      	"Enter your location for transmission\nto other participants." "user_loc.au"

#audio help
set i .prefs.pane.audio
help::add $i.dd.device.mdev "Selects preferred audio device." "audio_device.au"
help::add $i.dd.sampling.freq.mb \
                        "Sets the sampling rate of the audio device.\nThis changes the available codecs." "sampling_freq.au"
help::add $i.dd.sampling.ch_in.mb \
                        "Changes between mono and stereo audio input." "mono_stereo.au"

help::add $i.dd.cks.f.f.silence.upper.r0\
			 "All audio is transmitted when the input is unmuted." "suppress_silence_off.au"
help::add $i.dd.cks.f.f.silence.upper.r1\
			 "Only audio above an automatically determined threshold\nis transmitted when the input is unmuted." "suppress_silence_auto.au"
help::add $i.dd.cks.f.f.silence.upper.r2\
			 "Only audio above a manually set threshold is\ntransmitted when the input is unmuted." "suppress_silence_manual.au"
help::add $i.dd.cks.f.f.silence.lower.s\
			 "Sets the manual threshold silence detection threshold." "suppress_silence_manual_thresh.au"

help::add $i.dd.cks.f.f.other.agc	 "Enables automatic control of the volume\nof the sound you send." "agc.au"
help::add $i.dd.cks.f.f.other.loop "Enables hardware for loopback of audio input." "audio_loopback.au"
help::add $i.dd.cks.f.f.other.suppress \
                         "Mutes microphone when playing audio." "echo_suppression.au"
help::add $i.dd.cks.f.f.other.tone \
                         "Plays a local test tone (not transmitted)." "test_tone.au"

# transmission help
set i .prefs.pane.transmission

help::add $i.dd.units.m	"Sets the duration of each packet sent.\nThere is a fixed per-packet\
                         overhead, so\nmaking this larger will reduce the total\noverhead.\
                         The effects of packet loss are\nmore noticable with large packets." "packet_duration.au"
help::add $i.dd.pri.m	"Changes the primary audio compression\nscheme. The list is arranged\
                         with high-\nquality, high-bandwidth choices at the\ntop, and\
                         poor-quality, lower-bandwidth\nchoices at the bottom." "codec_primary.au"
help::add $i.cc.van.rb	"Sets no channel coding." "channel_coding_none.au"
help::add $i.cc.red.rb	"Piggybacks earlier units of audio into packets\n\
			 to protect against packet loss. Some audio\n\
			 tools (eg: vat-4.0) are not able to receive\n\
			 audio sent with this option." "channel_coding_redundant.au"
help::add $i.cc.red.fc.m \
			"Sets the format of the piggybacked data." "red_codec.au"
help::add $i.cc.red.u.m \
			"Sets the offset of the piggybacked data." "red_offset.au"
help::add $i.cc.layer.fc.m  "Sets the number of discrete layers which will\nbe sent. You need\
                            to start RAT with the options\n-l n <address>/<port> <address>/<port>,\nwhere\
                            n is the number of layers and there is an\naddress and port for each layer.\
                            NB: this is only\nsupported by the WBS codec at present." "num_layers.au"
help::add $i.cc.int.fc.m \
			"Sets the separation of adjacent units within\neach packet. Larger values correspond\
			 to longer\ndelays." "int_sep.au"
help::add $i.cc.int.zz.m "Number of compound units per packet." "int_units.au"
help::add $i.cc.int.rb	"Enables interleaving which exchanges latency\n\
			 for protection against burst losses.  No other\n\
			 audio tools can decode this format (experimental)." "channel_coding_interleave.au"

# Reception Help
set i .prefs.pane.reception
help::add $i.r.m 	"Sets the type of repair applied when packets are\nlost. The schemes\
			 are listed in order of increasing\ncomplexity and quality of repair." "repair_schemes.au"
help::add $i.r.ms        "Sets the type of sample rate conversion algorithm\n\
			 that will be applied to streams that differ in rate\n\
			 to the audio device rate." "samples_rate_conversion.au"
help::add $i.o.f.cb      "Enforce playout delay limits set below.\nThis is not usually desirable." "playout_limits.au"
help::add $i.o.f.fl.scmin   "Sets the minimum playout delay that will be\napplied to incoming\
			 audio streams." "min_playout_delay.au"
help::add $i.o.f.fr.scmax   "Sets the maximum playout delay that will be\napplied to incoming\
			 audio streams." "max_playout_delay.au"
help::add $i.c.f.f.lec  	"If enabled, extra delay is added at both sender and receiver.\nThis allows\
                         the receiver to better cope with host scheduling\nproblems, and the sender\
			 to perform better silence suppression.\nAs the name suggests, this option\
			 is intended for scenarios such\nas transmitting a lecture, where interactivity\
			 is less important\nthan quality." "lecture_mode.au"

# security...too obvious for balloon help!
help::add .prefs.pane.security.a.f.f.e "Due to government export restrictions\nhelp\
				       for this option is not available." "security.au"

# interface...ditto!
set i .prefs.pane.interface
help::add $i.a.f.f.power "Disable display of audio powermeters. This\nis only\
		 	 useful if you have a slow machine" "disable_powermeters.au"
help::add $i.a.f.f.balloon "If you can see this, balloon help\nis enabled. If not, it isn't." "balloon_help.au"
help::add $i.a.f.f.matrix  "Displays a chart showing the reception\nquality reported by all participants" "rqm_enable.au"
help::add $i.a.f.f.plist   "Hides the list of participants" "participant_list.au"

help::add .chart		"This chart displays the reception quality reported\n by all session\
                         participants. Looking along a row\n gives the quality with which that\
			 participant was\n received by all other participants in the session:\n green\
			 is good quality, orange medium quality, and\n red poor quality audio." "rqm.au"

proc rendezvous_with_media_engine {} {
	mbus_send "R" "tool.rat.settings" ""
	mbus_send "R" "audio.query" ""
	mbus_send "R" "rtp.query" ""
}

} script_evaluation_error ]

if {$script_error != 0} {
        bgerror "$script_evaluation_error"
}

catch {console hide}
