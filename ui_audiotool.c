char ui_audiotool[] = "\
namespace eval bargraph {\n\
	variable litColors   [list #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #2ccf00 #58d200 #84d500 #b0d800 #dddd00 #ddb000 #dd8300 #dd5600 #dd2900]\n\
	variable unlitColors [list #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #166700 #2c6900 #426a00 #586c00 #6e6e00 #6e5800 #6e4100 #6e2b00 #6e1400]\n\
	variable totalHeight [llength $litColors]\n\
	variable prevHeight\n\
\n\
	proc create {bgraph} {\n\
		variable oh$bgraph\n\
		variable totalHeight\n\
		variable unlitColors\n\
		variable prevHeight\n\
\n\
		frame $bgraph -bd 0 -bg black\n\
        	for {set i 0} {$i < $totalHeight} {incr i} {\n\
                	frame $bgraph.inner$i -bg \"[lindex $unlitColors $i]\" -width 4 -height 6\n\
                	pack  $bgraph.inner$i -side left -fill both -expand true -padx 1 -pady 1\n\
        	}\n\
        	set prevHeight($bgraph) 0\n\
	}\n\
\n\
	proc setHeight {bgraph height} {\n\
		variable prevHeight\n\
		variable totalHeight\n\
		variable litColors\n\
		variable unlitColors\n\
\n\
		if {$prevHeight($bgraph) > $height} {\n\
			for {set i [expr $height]} {$i <= $prevHeight($bgraph)} {incr i} {\n\
				$bgraph.inner$i config -bg \"[lindex $unlitColors $i]\"\n\
			}\n\
		} else {\n\
		    for {set i [expr $prevHeight($bgraph)]} {$i <= $height} {incr i} {\n\
			$bgraph.inner$i config -bg  \"[lindex $litColors $i]\"\n\
		    }\n\
		}\n\
		set prevHeight($bgraph) $height\n\
	}\n\
\n\
	proc disable {bgraph} {\n\
		variable totalHeight\n\
\n\
		if {![winfo exists $bgraph]} {\n\
			error \"Bar graph $bgraph does not exist\"\n\
		}\n\
		for {set i 0} {$i < $totalHeight} {incr i} {\n\
			$bgraph.inner$i config -bg black\n\
		}\n\
	}\n\
\n\
	proc enable {bgraph} {\n\
		variable totalHeight\n\
		variable unlitColors\n\
		variable litColors\n\
		variable prevHeight\n\
\n\
		if {![winfo exists $bgraph]} {\n\
			error \"Bar graph $bgraph does not exist\"\n\
		}\n\
		for {set i 0} {$i < $totalHeight} {incr i} {\n\
			$bgraph.inner$i config -bg \"[lindex $unlitColors $i]\"\n\
		}\n\
		for {set i 0} {$i < $prevHeight($bgraph)} {incr i} {\n\
			$bgraph.inner$i config -bg \"[lindex $litColors $i]\"\n\
		}\n\
	}\n\
}\n\
\n\
namespace eval chart {\n\
	proc create {} {\n\
		toplevel  .chart\n\
		canvas    .chart.c  -xscrollcommand {.chart.sb set} -yscrollcommand {.chart.sr set}\n\
		frame     .chart.c.f\n\
		if {$::tk_version < 8.5} {\n\
			scrollbar .chart.sr -orient vertical   -command {.chart.c yview}\n\
			scrollbar .chart.sb -orient horizontal -command {.chart.c xview}\n\
		} else {\n\
			ttk::scrollbar .chart.sr -orient vertical   -command {.chart.c yview}\n\
			ttk::scrollbar .chart.sb -orient horizontal -command {.chart.c xview}\n\
		}\n\
		pack .chart.sb -side bottom -fill x    -expand 0 -anchor s\n\
		pack .chart.sr -side right  -fill y    -expand 0 -anchor e\n\
		pack .chart.c  -side left   -fill both -expand 1 -anchor n\n\
\n\
		.chart.c create window 0 0 -anchor nw -window .chart.c.f\n\
\n\
		toplevel .chart_popup       -bg black\n\
		label    .chart_popup.text  -bg lavender -justify left\n\
		pack .chart_popup.text -side top -anchor w -fill x\n\
		wm transient        .chart_popup .\n\
		wm withdraw         .chart_popup\n\
		wm overrideredirect .chart_popup true\n\
\n\
		wm withdraw .chart\n\
		wm title    .chart \"Reception quality matrix\"\n\
		wm geometry .chart 320x200\n\
		wm protocol .chart WM_DELETE_WINDOW chart::hide\n\
	}\n\
\n\
	# Private - do not use outside this namespace\n\
	proc popup_show {window} {\n\
		variable chart_popup_src\n\
		variable chart_popup_dst\n\
		variable chart_popup_id\n\
		global   NAME\n\
		.chart_popup.text  configure -text \"From: $NAME($chart_popup_src($window))\\nTo: $NAME($chart_popup_dst($window))\"\n\
		# Beware! Don't put the popup under the cursor! Else we get window enter\n\
		# for .help and leave for $window, making us hide_help which removes the\n\
		# help window, giving us a window enter for $window making us popup the\n\
		# help again.....\n\
		if {[winfo width $window] > [winfo height $window]} {\n\
			set xpos [expr [winfo pointerx $window] + 10]\n\
			set ypos [expr [winfo rooty    $window] + [winfo height $window] + 5]\n\
		} else {\n\
			set xpos [expr [winfo rootx    $window] + [winfo width $window] + 5]\n\
			set ypos [expr [winfo pointery $window] + 10]\n\
		}\n\
\n\
		wm geometry  .chart_popup +$xpos+$ypos\n\
		set chart_popup_id [after 100 wm deiconify .chart_popup]\n\
		raise .chart_popup $window\n\
	}\n\
\n\
	# Private - do not use outside this namespace\n\
	proc popup_hide {window} {\n\
		variable chart_popup_id\n\
		if {[info exists chart_popup_id]} {\n\
			after cancel $chart_popup_id\n\
		}\n\
		wm withdraw .chart_popup\n\
	}\n\
\n\
	# Private - do not use outside this namespace\n\
	proc popup_add {window src dst} {\n\
		variable chart_popup_src\n\
		variable chart_popup_dst\n\
		set chart_popup_src($window) $src\n\
		set chart_popup_dst($window) $dst\n\
		bind $window <Enter>    \"+chart::popup_show $window\"\n\
		bind $window <Leave>    \"+chart::popup_hide $window\"\n\
	}\n\
\n\
	proc add {ssrc} {\n\
		global NAME\n\
		frame  .chart.c.f.$ssrc\n\
		frame  .chart.c.f.$ssrc.f\n\
		button .chart.c.f.$ssrc.l -width 25 -text $ssrc -padx 0 -pady 0 -anchor w -relief flat -command \"toggle_stats $ssrc\"\n\
		pack   .chart.c.f.$ssrc.f -expand 0 -anchor e -side right\n\
		pack   .chart.c.f.$ssrc.l -expand 1 -anchor w -fill x -side left\n\
		pack   .chart.c.f.$ssrc   -expand 1 -anchor n -fill x -side top\n\
		foreach s [pack slaves .chart.c.f] {\n\
			regsub {.chart.c.f.(.*)} $s {\\1} s\n\
			button .chart.c.f.$s.f.$ssrc -width 5 -text \"\" -background white -padx 0 -pady 0 -command \"mtrace $s $ssrc\"\n\
			pack   .chart.c.f.$s.f.$ssrc -expand 0 -side left\n\
			chart::popup_add .chart.c.f.$s.f.$ssrc $s $ssrc\n\
			if {![winfo exists .chart.c.f.$ssrc.f.$s]} {\n\
				button .chart.c.f.$ssrc.f.$s -width 5 -text \"\" -background white -padx 0 -pady 0 -command \"mtrace $ssrc $s\"\n\
				pack   .chart.c.f.$ssrc.f.$s -expand 0 -side left\n\
				chart::popup_add .chart.c.f.$ssrc.f.$s $ssrc $s\n\
			}\n\
		}\n\
		update\n\
		.chart.c configure -scrollregion \"0.0 0.0 [winfo width .chart.c.f] [winfo height .chart.c.f]\"\n\
	}\n\
\n\
	proc remove {ssrc} {\n\
		destroy .chart.c.f.$ssrc\n\
		foreach s [pack slaves .chart.c.f] {\n\
			regsub {.chart.c.f.(.*)} $s {\\1} s\n\
			destroy .chart.c.f.$s.f.$ssrc\n\
		}\n\
		update\n\
		.chart.c configure -scrollregion \"0.0 0.0 [winfo width .chart.c.f] [winfo height .chart.c.f]\"\n\
	}\n\
\n\
	proc setlabel {ssrc label} {\n\
		.chart.c.f.$ssrc.l configure -text $label\n\
	}\n\
\n\
	proc setvalue {src dst val} {\n\
		if {$val < 5} {\n\
			set colour green\n\
			set txtval \"$val%\"\n\
		} elseif {$val < 10} {\n\
			set colour orange\n\
			set txtval \"$val%\"\n\
		} elseif {$val <= 100} {\n\
			set colour red\n\
			set txtval \"$val%\"\n\
		} else {\n\
			set colour white\n\
			set txtval { }\n\
		}\n\
		.chart.c.f.$src.f.$dst configure -highlightbackground $colour -bg $colour -text \"$txtval\"\n\
	}\n\
\n\
	proc show {} {\n\
		wm deiconify .chart\n\
	}\n\
\n\
	proc hide {} {\n\
		wm withdraw .chart\n\
	}\n\
}\n\
namespace eval help {\n\
	global help_on\n\
	set help_on 0\n\
\n\
	proc show {window} {\n\
		variable help_text\n\
		global help_on\n\
		variable help_id\n\
		variable help_clip\n\
		if {$help_on} {\n\
			.help.text  configure -text $help_text($window)\n\
			# Beware! Don't put the popup under the cursor! Else we get window enter\n\
			# for .help and leave for $window, making us hide_help which removes the\n\
			# help window, giving us a window enter for $window making us popup the\n\
			# help again.....\n\
			if {[winfo width $window] > [winfo height $window]} {\n\
			    set xpos [expr [winfo pointerx $window] + 10]\n\
			    set ypos [expr [winfo rooty    $window] + [winfo height $window] + 4]\n\
			} else {\n\
			    set xpos [expr [winfo rootx    $window] + [winfo width $window] + 4]\n\
			    set ypos [expr [winfo pointery $window] + 10]\n\
			}\n\
\n\
			wm geometry  .help +$xpos+$ypos\n\
			set help_id [after 100 wm deiconify .help]\n\
			raise .help $window\n\
			if {[info exists help_clip($window)]} {\n\
			    mbus_send \"R\" \"tool.rat.voxlet.play\" \"\\\"$help_clip($window)\\\"\"\n\
			}\n\
		}\n\
	}\n\
\n\
	proc hide {window} {\n\
		variable help_id\n\
		global help_on\n\
		if {[info exists help_id]} {\n\
			after cancel $help_id\n\
		}\n\
		wm withdraw .help\n\
	}\n\
\n\
	proc add {window text clip} {\n\
		variable help_text\n\
		variable help_clip\n\
		set help_text($window)  $text\n\
		set help_clip($window)  $clip\n\
		bind $window <Enter>    \"+help::show $window\"\n\
		bind $window <Leave>    \"+help::hide $window\"\n\
	}\n\
\n\
	bind Entry   <KeyPress> \"+help::hide %W\"\n\
	toplevel .help       -bg black\n\
	label    .help.text  -bg lavender -justify left\n\
	pack .help.text -side top -anchor w -fill x\n\
	wm transient        .help .\n\
	wm withdraw         .help\n\
	wm overrideredirect .help true\n\
}\n\
\n\
\n\
\n\
# Asynchronous file box -the default one is modal and no good for real time applications.\n\
#\n\
# Usage: asFileBox .widget\n\
#\n\
# Options: -command <command>\n\
#          -defaultextension <extension>\n\
#          -directory  <startdir>\n\
#          -extensions <extension list>\n\
#          -force_extension <bool>       - append extension to name if not entered\n\
#          -type    <button label>       - Open/Save\n\
#          -title <title>\n\
# E.g.\n\
# set extensions {{\"NeXT/Sun Audio Files\" \"au\"} {\"Scriptics TCL Files\" \"tcl\"} {\"Microsoft RIFF files\" \"wav\"} {\"All Files\" \"*\"}}\n\
# asFileBox .zzz -command puts -extensions $extensions -defaultextension au\n\
\n\
proc asParseArgs {dstArray commands options} {\n\
    upvar $dstArray out\n\
    upvar $options opts\n\
\n\
    # Keep note of default values\n\
    foreach i $commands {\n\
	set field [lindex $i 0]\n\
	set value [lindex $i 1]\n\
	set out($field) $value\n\
    }\n\
\n\
    # Read all options -whatever value\n\
    for {set i 0} {$i < [llength $opts]} {incr i} {\n\
	if {[string index [lindex $opts $i] 0] == \"-\"} {\n\
	    set out([lindex $opts $i]) [lindex $opts [incr i]]\n\
	}\n\
    }\n\
}\n\
\n\
# Returns .root from .root.sub1.sub2.etc\n\
proc asRootName {path} {\n\
    set out [split $path .]\n\
    return .[lindex $out 1]\n\
}\n\
\n\
proc asUpdir {path} {\n\
    set l [split [string trimright $path /] /]\n\
    set end [expr [llength $l] - 1]\n\
\n\
    if {$end <= 0} {\n\
	return $path\n\
    }\n\
\n\
    for {set i 0} {$i < $end} {incr i} {\n\
	append updir [lindex $l $i] /\n\
    }\n\
    return $updir\n\
}\n\
\n\
proc asSelectFile {w value} {\n\
    global as_cwd\n\
\n\
    $w.main.entries.e delete 0 end\n\
\n\
    if [file isfile $as_cwd($w)/$value] {\n\
	$w.main.entries.e insert 0 $value\n\
    }\n\
}\n\
\n\
proc asOpenFile {w value} {\n\
    global as_cwd as_ok as_file\n\
\n\
    set value [string trimright $value /]\n\
    set value [string trim $value \\[\\]]\n\
    if {$value == \"..\"} {\n\
		catch {\n\
			set as_cwd($w) [asUpdir $as_cwd($w)]\n\
			cd $as_cwd($w)\n\
		}\n\
		asFilter $w\n\
    } elseif [string match \"?:\" $value] {\n\
		catch {\n\
			cd $value/\n\
			set as_cwd($w) [pwd]\n\
		}\n\
		asFilter $w\n\
    } elseif [file isdirectory $as_cwd($w)/$value] {\n\
		set as_file($w) \"\"\n\
		catch {\n\
		    cd $as_cwd($w)/$value\n\
		    set as_cwd($w) [pwd]\n\
		}\n\
		asFilter $w\n\
    } elseif [file isfile $as_cwd($w)/$value] {\n\
		tkButtonInvoke $as_ok($w)\n\
    }\n\
}\n\
\n\
proc asFixName {w} {\n\
    global as_ext as_file\n\
\n\
    if {[string compare [string tolower [file extension $as_file($w)] ] .$as_ext($w)] \\\n\
	    && $as_ext($w) != \"*\" } {\n\
	set as_file($w) \"$as_file($w).$as_ext($w)\"\n\
    }\n\
}\n\
\n\
proc asCommand {w} {\n\
    global as_cwd as_ext as_list as_file as_cmd as_ok as_fix_name\n\
\n\
# Only perform actions if file exists\n\
    if {$as_file($w) != \"\"} {\n\
	if $as_fix_name($w) {\n\
	    asFixName $w\n\
	}\n\
	$as_cmd($w) $as_cwd($w)/$as_file($w)\n\
    }\n\
\n\
    unset as_cwd($w)\n\
    unset as_ext($w)\n\
    unset as_list($w)\n\
    unset as_file($w)\n\
    unset as_cmd($w)\n\
    unset as_ok($w)\n\
    unset as_fix_name($w)\n\
\n\
    destroy $w\n\
}\n\
\n\
proc asFilter {w} {\n\
    global as_cwd as_ext as_list tcl_platform\n\
\n\
    $as_list($w) delete 0 end\n\
\n\
    cd $as_cwd($w)\n\
\n\
    # Check if directory above exists\n\
    set updir [asUpdir $as_cwd($w)]\n\
\n\
    if {$updir != $as_cwd($w)} {\n\
	set allfiles \".. [lsort [glob -nocomplain *]]\"\n\
    } else {\n\
	set allfiles \"[lsort [glob -nocomplain *]]\"\n\
	if {[string match \"Windows*\" \"$tcl_platform(os)\"]} {\n\
	    set drives [file volume]\n\
	}\n\
    }\n\
\n\
    set ext $as_ext($w)\n\
\n\
    # Split into files and directories\n\
    foreach f $allfiles {\n\
	#This stops windows getting excited about paths beginning ~\n\
	if {[file exists $f]} {\n\
	    if {[file isdirectory $f]} {\n\
		lappend dirs \"$f/\"\n\
	    } elseif {[file isfile $f]} {\n\
		if {$ext == \"*\"} {\n\
		    lappend files $f\n\
		} elseif {[string tolower [file extension $f]] == \".$ext\"} {\n\
		    lappend files $f\n\
		}\n\
	    }\n\
	}\n\
    }\n\
    if [info exists drives] {\n\
	foreach f $drives {\n\
	    $as_list($w) insert end \"\\[[string trimright $f /]\\]\"\n\
	}\n\
    }\n\
\n\
    if [info exists dirs] {\n\
	foreach f $dirs {\n\
	    $as_list($w) insert end \"[file tail $f]/\"\n\
	}\n\
    }\n\
    if [info exists files] {\n\
	foreach f $files {\n\
	    $as_list($w) insert end $f\n\
	}\n\
    }\n\
}\n\
\n\
proc asFileBox {w args} {\n\
    upvar $w base\n\
    global as_cwd as_ext as_list as_file as_cmd as_ok as_fix_name\n\
\n\
    # Process options\n\
    set commands {\n\
	{\"-command\"          \"puts\"}\n\
	{\"-defaultextension\" \"\"}\n\
	{\"-directory\"        \".\"}\n\
	{\"-extensions\"       \"\"}\n\
	{\"-force_extension\"   0}\n\
	{\"-title\"            \"Open File\"}\n\
	{\"-type\"             \"Open\"}\n\
    }\n\
\n\
    asParseArgs base $commands args\n\
\n\
    # Save command\n\
    set as_cmd($w) $base(-command)\n\
    set as_file($w) \"\"\n\
\n\
    # Do we return name with extension appended ?\n\
    set as_fix_name($w) $base(-force_extension)\n\
\n\
    # Create Window\n\
    toplevel $w\n\
    wm title $w $base(-title)\n\
    wm geometry $w 400x320\n\
    wm resizable $w 0 0\n\
\n\
    frame $w.main\n\
    pack  $w.main -fill both -expand 1 -padx 4 -pady 4\n\
\n\
    # Create Path display\n\
    frame $w.main.path\n\
    label $w.main.path.l -text \"Path: \"\n\
    label $w.main.path.lp  -justify left -textvariable as_cwd($w)\n\
    pack  $w.main.path -fill x -expand 0\n\
    pack  $w.main.path.l  -side left\n\
    pack  $w.main.path.lp -side left -anchor e\n\
\n\
    # Create file list\n\
    frame $w.main.lbf\n\
    if {$::tk_version < 8.5} {\n\
	scrollbar $w.main.lbf.scroll -command \"$w.main.lbf.list yview\"\n\
    } else {\n\
	ttk::scrollbar $w.main.lbf.scroll -command \"$w.main.lbf.list yview\"\n\
    }\n\
    set as_list($w) [listbox $w.main.lbf.list -yscroll \"$w.main.lbf.scroll set\" -bg white]\n\
    pack $w.main.lbf -side top -expand 1 -fill both\n\
    pack $w.main.lbf.scroll -side right -fill y\n\
    pack $w.main.lbf.list -side left -expand 1 -fill both\n\
\n\
    # Create Frame for Labels\n\
    frame $w.main.labels\n\
    pack  $w.main.labels -side left\n\
    label $w.main.labels.filename -text \"File name:\"\n\
    label $w.main.labels.types -text \"File type:\"\n\
    pack  $w.main.labels.filename $w.main.labels.types -side top -fill x -ipady 4\n\
\n\
    # Add Entryframe and file types menubutton\n\
    frame $w.main.entries\n\
    entry $w.main.entries.e -textvariable as_file($w) -bg white\n\
    menubutton $w.main.entries.mb -menu $w.main.entries.mb.menu  -indicatoron 1 -relief raised -width 20 -justify left\n\
    menu $w.main.entries.mb.menu -tearoff 0\n\
    pack $w.main.entries -side left -fill x -expand 1 -ipady 8 -ipadx 8 -padx 8\n\
    pack $w.main.entries.e $w.main.entries.mb -side top -fill x -expand 1\n\
\n\
    # Pack buttons\n\
    frame $w.main.cmdf\n\
    set as_ok($w) [button $w.main.cmdf.ok -text $base(-type) -command \"asCommand $w\"]\n\
    button $w.main.cmdf.cancel -text \"Cancel\" -command \"destroy $w\"\n\
    pack $w.main.cmdf -side right -fill both -expand 0\n\
    pack $w.main.cmdf.ok     -side top    -expand 1 -fill x\n\
    pack $w.main.cmdf.cancel -side bottom -expand 1 -fill x\n\
\n\
    # Fill in extensions\n\
    if {$base(-defaultextension) == \"\"} {\n\
	set base(-defaultextension) [lindex [lindex $base(-extensions) 0] 1]\n\
	puts \"$base(-defaultextension)\"\n\
    }\n\
\n\
    foreach i $base(-extensions) {\n\
	set wording [lindex $i 0]\n\
	set ext     [lindex $i 1]\n\
	$w.main.entries.mb.menu add command -label \"$wording (*.$ext)\" -command \"set as_ext($w) $ext; asFilter $w; $w.main.entries.mb configure -text \\\"$wording (*.$ext)\\\"\"\n\
	if {$ext == $base(-defaultextension)} {\n\
	    $w.main.entries.mb configure -text \"$wording (*.$ext)\"\n\
	}\n\
    }\n\
\n\
    # Set up global storage for variables\n\
    set as_cwd($w) [pwd]\n\
    if {$base(-directory) != \".\"} {\n\
	set as_cwd($w) $base(-directory)\n\
    }\n\
\n\
    set as_ext($w) $base(-defaultextension)\n\
\n\
	# Add listbox bindings\n\
    bind $w.main.lbf.list <1> {\n\
		%W activate @%x,%y\n\
		asSelectFile [asRootName %W] [%W get active]\n\
    }\n\
\n\
    bind $w.main.lbf.list <Double-Button-1> {\n\
		%W activate @%x,%y\n\
		asOpenFile [asRootName %W] [%W get active]\n\
    }\n\
\n\
	bind $w.main.lbf.list <KeyPress-Return> {\n\
\n\
		asOpenFile [asRootName %W] [%W get active]\n\
    }\n\
\n\
	bind $w <Up> {\n\
		set sel [%W.main.lbf.list curselection]\n\
		if {$sel != {}} {\n\
			incr sel -1\n\
			if { $sel > -1 } {\n\
				%W.main.lbf.list selection clear 0 end\n\
				%W.main.lbf.list selection set $sel\n\
				%W.main.lbf.list yview $sel\n\
			asSelectFile [asRootName %W] [%W.main.lbf.list get active]\n\
			}\n\
		}\n\
    }\n\
\n\
	bind $w <Down> {\n\
		set sel [%W.main.lbf.list curselection]\n\
		if {$sel != {}} {\n\
			incr sel\n\
			if { $sel < [%W.main.lbf.list index end] } {\n\
				%W.main.lbf.list selection clear 0 end\n\
				%W.main.lbf.list selection set $sel\n\
				%W.main.lbf.list yview $sel\n\
				asSelectFile [asRootName %W] [%W.main.lbf.list get $sel]\n\
			}\n\
		}\n\
    }\n\
\n\
    asFilter $w\n\
}\n\
\n\
\n\
\n\
\n\
#\n\
# Copyright (c) 1995-2002 University College London\n\
# All rights reserved.\n\
#\n\
# $Revision: 4547 $\n\
#\n\
# Full terms and conditions of the copyright appear below.\n\
#\n\
\n\
package require Tk\n\
\n\
if {$::tk_version > 8.4} {\n\
    package require Ttk\n\
    ttk::style theme use clam\n\
}\n\
\n\
set script_error [ catch {\n\
\n\
wm withdraw .\n\
\n\
if {[string compare [info commands registry] \"registry\"] == 0} {\n\
    set win32 1\n\
} else {\n\
    set win32 0\n\
    if {$::tk_version < 8.5} {\n\
	option add *Menu*selectColor		forestgreen\n\
	option add *Radiobutton*selectColor	forestgreen\n\
	option add *Checkbutton*selectColor	forestgreen\n\
	option add *Entry.background		gray70\n\
    }\n\
}\n\
\n\
set statsfont     [font actual {helvetica 10}]\n\
set compfont      [font actual {helvetica 10}]\n\
set titlefont     [font actual {helvetica 10}]\n\
set infofont      [font actual {helvetica 10}]\n\
set smallfont     [font actual {helvetica  8}]\n\
set verysmallfont [font actual {helvetica  8}]\n\
\n\
set speaker_highlight white\n\
\n\
option add *Entry.relief       sunken\n\
option add *borderWidth        1\n\
option add *highlightThickness 0\n\
option add *font               $infofont\n\
option add *Menu*tearOff       0\n\
\n\
set V(class) \"Mbone Applications\"\n\
set V(app)   \"rat\"\n\
\n\
set iht			16\n\
set iwd 		280\n\
set cancel_info_timer 	0\n\
set num_ssrc		0\n\
set fw			.l.t.list.f\n\
set iports             [list]\n\
set oports             [list]\n\
set bps_in              \"0.0 b/s\"\n\
set bps_out             $bps_in\n\
\n\
# Sliders use scale widget and this invokes it's -command option when\n\
# it is created (doh!)  We don't know what gain and volume settings\n\
# should be before we've received info from the audio engine.\n\
# Don't send our initial value if we've not heard anything from engine\n\
# first.  Otherwise we get catch-22 with both engine and ui queuing\n\
# messages of gains to each other and often UI wins, which it never\n\
# should do.\n\
\n\
set received(gain)      0\n\
set received(volume)    0\n\
\n\
###############################################################################\n\
#\n\
# Debugging Functions\n\
#\n\
###############################################################################\n\
\n\
# tvar - trace variable callback for watching changes to variables\n\
# e.g. to watch write events to myVar use:\n\
#                                          trace variable myVar w tvar\n\
\n\
proc tvar {name element op} {\n\
# Compose variable name and print out change\n\
    if {$element != \"\"} {\n\
	set name ${name}($element)\n\
    }\n\
    upvar $name x\n\
    puts stderr \"Variable $name set to $x.\"\n\
\n\
# Unravel stack and display\n\
    puts \"Stack trace:\"\n\
    set depth [info level]\n\
    set l 0\n\
    while {$l < $depth} {\n\
	puts stderr \"\\tLevel $l: [info level $l]\"\n\
	incr l\n\
    }\n\
}\n\
\n\
###############################################################################\n\
\n\
# trace variable gain w tvar\n\
\n\
proc init_source {ssrc} {\n\
	global CNAME NAME EMAIL LOC PHONE TOOL NOTE PRIV SSRC num_ssrc\n\
	global CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP \\\n\
               JITTER LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED BUFFER_SIZE \\\n\
	       PLAYOUT_DELAY GAIN MUTE SKEW SPIKE_EVENTS SPIKE_TOGED RTT\n\
\n\
	# This is a debugging test -- old versions of the mbus used the\n\
	# cname to identify participants, whilst the newer version uses\n\
	# the ssrc.  This check detects if old style commands are being\n\
	# used and raises an error if so.\n\
	if [regexp {.*@[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+} \"$ssrc\"] {\n\
		error \"ssrc $ssrc invalid\"\n\
	}\n\
\n\
	if {[array names INDEX $ssrc] != [list $ssrc]} {\n\
		# This is a source we've not seen before...\n\
		set         CNAME($ssrc) \"\"\n\
		set          NAME($ssrc) $ssrc\n\
		set         EMAIL($ssrc) \"\"\n\
		set         PHONE($ssrc) \"\"\n\
		set           LOC($ssrc) \"\"\n\
		set          TOOL($ssrc) \"\"\n\
		set          NOTE($ssrc) \"\"\n\
		set          PRIV($ssrc) \"\"\n\
		set         CODEC($ssrc) unknown\n\
		set          GAIN($ssrc) 1.0\n\
		set          MUTE($ssrc) 0\n\
		set      DURATION($ssrc) \"\"\n\
		set   BUFFER_SIZE($ssrc) 0\n\
		set PLAYOUT_DELAY($ssrc) 0\n\
		set          SKEW($ssrc) 1.000\n\
		set  SPIKE_EVENTS($ssrc) 0\n\
		set   SPIKE_TOGED($ssrc) 0\n\
		set           RTT($ssrc) \"\"\n\
		set    PCKTS_RECV($ssrc) 0\n\
		set    PCKTS_LOST($ssrc) 0\n\
		set    PCKTS_MISO($ssrc) 0\n\
		set     PCKTS_DUP($ssrc) 0\n\
		set        JITTER($ssrc) 0\n\
		set     JIT_TOGED($ssrc) 0\n\
		set    LOSS_TO_ME($ssrc) 0\n\
		set  LOSS_FROM_ME($ssrc) 0\n\
		set  HEARD_LOSS_TO_ME($ssrc) 0\n\
		set  HEARD_LOSS_FROM_ME($ssrc) 0\n\
		set         INDEX($ssrc) $num_ssrc\n\
		set          SSRC($ssrc) $ssrc\n\
		incr num_ssrc\n\
		chart::add   $ssrc\n\
	}\n\
}\n\
\n\
proc window_plist {ssrc} {\n\
    	global fw\n\
	regsub -all {@|\\.} $ssrc {-} foo\n\
	return $fw.source-$foo\n\
}\n\
\n\
proc window_stats {ssrc} {\n\
	regsub -all {[\\. ]} $ssrc {-} foo\n\
	return .stats$foo\n\
}\n\
\n\
# Commands to send message over the conference bus...\n\
proc output_mute {state} {\n\
    mbus_send \"R\" \"audio.output.mute\" \"$state\"\n\
    if {$state} {\n\
    	bargraph::disable .r.c.rx.au.pow.bar\n\
    } else {\n\
        bargraph::enable .r.c.rx.au.pow.bar\n\
    }\n\
}\n\
\n\
proc input_mute {state} {\n\
    mbus_send \"R\" \"audio.input.mute\" \"$state\"\n\
    if {$state} {\n\
    	bargraph::disable .r.c.tx.au.pow.bar\n\
    } else {\n\
        bargraph::enable .r.c.tx.au.pow.bar\n\
    }\n\
}\n\
\n\
proc set_vol {new_vol} {\n\
    global volume received\n\
    set volume $new_vol\n\
    if {$received(volume)} {\n\
	mbus_send \"R\" \"audio.output.gain\" $volume\n\
    }\n\
}\n\
\n\
proc set_gain {new_gain} {\n\
    global gain received\n\
    set gain $new_gain\n\
    if {$received(gain)} {\n\
	mbus_send \"R\" \"audio.input.gain\" $gain\n\
    }\n\
}\n\
\n\
proc toggle_iport {} {\n\
    global iport iports\n\
\n\
    set len [llength $iports]\n\
# lsearch returns -1 if not found, index otherwise\n\
    set idx [lsearch -exact $iports $iport]\n\
\n\
    if {$idx != -1} {\n\
	incr idx\n\
	set idx [expr $idx % $len]\n\
	set port [lindex $iports $idx]\n\
	mbus_send \"R\" \"audio.input.port\" [mbus_encode_str $port]\n\
    }\n\
}\n\
\n\
proc incr_port {varname listname delta} {\n\
    upvar $varname  port\n\
    upvar $listname ports\n\
    set len [llength $ports]\n\
    # lsearch returns -1 if not found, index otherwise\n\
    set idx [lsearch -exact $ports $port]\n\
\n\
    if {$idx != -1} {\n\
	incr idx $delta\n\
	if {$idx < 0} {\n\
	    set idx [expr $len - 1]\n\
	} elseif {$idx >= $len} {\n\
	    set idx 0\n\
	}\n\
	set port [lindex $ports $idx]\n\
# yuechh!\n\
	if {$varname == \"iport\"} {\n\
	    mbus_send \"R\" \"audio.input.port\" [mbus_encode_str $port]\n\
	} else {\n\
	    mbus_send \"R\" \"audio.output.port\" [mbus_encode_str $port]\n\
	}\n\
    }\n\
}\n\
\n\
proc mbus_heartbeat {} {\n\
}\n\
\n\
#############################################################################################################\n\
# Reception of Mbus messages...\n\
\n\
proc mbus_recv {cmnd args} {\n\
	# This is not the most efficient way of doing this, since we could call mbus_recv_...\n\
	# directly from the C code. It does, however, make it explicit which Mbus commands we\n\
	# understand.\n\
	switch $cmnd {\n\
		mbus.waiting			{eval mbus_recv_mbus.waiting $args}\n\
		mbus.go				{eval mbus_recv_mbus.go $args}\n\
		mbus.hello			{eval mbus_recv_mbus.hello $args}\n\
		mbus.quit  			{eval mbus_recv_mbus.quit $args}\n\
		tool.rat.sampling.supported 	{eval mbus_recv_tool.rat.sampling.supported $args}\n\
		tool.rat.converter  		{eval mbus_recv_tool.rat.converter $args}\n\
		tool.rat.converters.flush 	{eval mbus_recv_tool.rat.converters.flush $args}\n\
		tool.rat.converters.add 	{eval mbus_recv_tool.rat.converters.add $args}\n\
		tool.rat.repairs.flush		{eval mbus_recv_tool.rat.repairs.flush $args}\n\
		tool.rat.repairs.add		{eval mbus_recv_tool.rat.repairs.add $args}\n\
		tool.rat.powermeter		{eval mbus_recv_tool.rat.powermeter $args}\n\
		tool.rat.agc  			{eval mbus_recv_tool.rat.agc $args}\n\
		tool.rat.format.in              {eval mbus_recv_tool.rat.format.in $args}\n\
		tool.rat.codec  		{eval mbus_recv_tool.rat.codec $args}\n\
		tool.rat.codecs.flush           {eval mbus_recv_tool.rat.codecs.flush $args}\n\
		tool.rat.codecs.add             {eval mbus_recv_tool.rat.codecs.add $args}\n\
		tool.rat.rate  			{eval mbus_recv_tool.rat.rate $args}\n\
		tool.rat.lecture.mode  		{eval mbus_recv_tool.rat.lecture.mode $args}\n\
		tool.rat.audio.buffered  	{eval mbus_recv_tool.rat.audio.buffered $args}\n\
		tool.rat.audio.delay    	{eval mbus_recv_tool.rat.audio.delay $args}\n\
		tool.rat.audio.skew     	{eval mbus_recv_tool.rat.audio.skew $args}\n\
		tool.rat.spike.events    	{eval mbus_recv_tool.rat.spike.events $args}\n\
		tool.rat.spike.toged     	{eval mbus_recv_tool.rat.spike.toged $args}\n\
		tool.rat.silence                {eval mbus_recv_tool.rat.silence $args}\n\
		tool.rat.silence.threshold      {eval mbus_recv_tool.rat.silence.threshold $args}\n\
		audio.3d.enabled  		{eval mbus_recv_audio.3d.enabled $args}\n\
		audio.3d.azimuth.min            {eval mbus_recv_audio.3d.azimuth.min $args}\n\
		audio.3d.azimuth.max            {eval mbus_recv_audio.3d.azimuth.max $args}\n\
		audio.3d.filter.types           {eval mbus_recv_audio.3d.filter.types   $args}\n\
		audio.3d.filter.lengths         {eval mbus_recv_audio.3d.filter.lengths $args}\n\
		audio.3d.user.settings          {eval mbus_recv_audio.3d.user.settings  $args}\n\
		tool.rat.playout.limit          {eval mbus_recv_tool.rat.playout.limit $args}\n\
		tool.rat.playout.min            {eval mbus_recv_tool.rat.playout.min   $args}\n\
		tool.rat.playout.max            {eval mbus_recv_tool.rat.playout.max   $args}\n\
		tool.rat.echo.suppress          {eval mbus_recv_tool.rat.echo.suppress $args}\n\
		tool.rat.loopback.gain          {eval mbus_recv_tool.rat.loopback.gain      $args}\n\
		tool.rat.bps.in                 {eval mbus_recv_tool.rat.bps.in        $args}\n\
		tool.rat.bps.out                {eval mbus_recv_tool.rat.bps.out       $args}\n\
		audio.suppress.silence  	{eval mbus_recv_audio.suppress.silence $args}\n\
		audio.channel.coding  		{eval mbus_recv_audio.channel.coding $args}\n\
		audio.channel.repair 		{eval mbus_recv_audio.channel.repair $args}\n\
		audio.devices.flush             {eval mbus_recv_audio_devices_flush $args}\n\
		audio.devices.add               {eval mbus_recv_audio_devices_add $args}\n\
		audio.device                    {eval mbus_recv_audio_device $args}\n\
		audio.input.gain  		{eval mbus_recv_audio.input.gain $args}\n\
		audio.input.port  		{eval mbus_recv_audio.input.port $args}\n\
		audio.input.ports.add           {eval mbus_recv_audio.input.ports.add $args}\n\
		audio.input.ports.flush         {eval mbus_recv_audio.input.ports.flush $args}\n\
		audio.input.mute  		{eval mbus_recv_audio.input.mute $args}\n\
		audio.input.powermeter  	{eval mbus_recv_audio.input.powermeter $args}\n\
		audio.output.gain  		{eval mbus_recv_audio.output.gain $args}\n\
		audio.output.port  		{eval mbus_recv_audio.output.port $args}\n\
		audio.output.ports.add          {eval mbus_recv_audio.output.ports.add $args}\n\
		audio.output.ports.flush        {eval mbus_recv_audio.output.ports.flush $args}\n\
		audio.output.mute  		{eval mbus_recv_audio.output.mute $args}\n\
		audio.output.powermeter  	{eval mbus_recv_audio.output.powermeter $args}\n\
		audio.file.play.ready   	{eval mbus_recv_audio.file.play.ready   $args}\n\
		audio.file.play.alive   	{eval mbus_recv_audio.file.play.alive $args}\n\
		audio.file.record.ready 	{eval mbus_recv_audio.file.record.ready $args}\n\
		audio.file.record.alive 	{eval mbus_recv_audio.file.record.alive $args}\n\
		session.title  			{eval mbus_recv_session.title $args}\n\
		rtp.addr  			{eval mbus_recv_rtp.addr $args}\n\
		rtp.ssrc  			{eval mbus_recv_rtp.ssrc $args}\n\
		rtp.source.exists  		{eval mbus_recv_rtp.source.exists $args}\n\
		rtp.source.remove  		{eval mbus_recv_rtp.source.remove $args}\n\
		rtp.source.cname  		{eval mbus_recv_rtp.source.cname $args}\n\
		rtp.source.name  		{eval mbus_recv_rtp.source.name $args}\n\
		rtp.source.email  		{eval mbus_recv_rtp.source.email $args}\n\
		rtp.source.phone  		{eval mbus_recv_rtp.source.phone $args}\n\
		rtp.source.loc  		{eval mbus_recv_rtp.source.loc $args}\n\
		rtp.source.tool  		{eval mbus_recv_rtp.source.tool $args}\n\
		rtp.source.note  		{eval mbus_recv_rtp.source.note $args}\n\
		rtp.source.priv  		{eval mbus_recv_rtp.source.priv $args}\n\
		rtp.source.codec  		{eval mbus_recv_rtp.source.codec $args}\n\
		rtp.source.packet.duration  	{eval mbus_recv_rtp.source.packet.duration $args}\n\
		rtp.source.packet.loss  	{eval mbus_recv_rtp.source.packet.loss $args}\n\
		rtp.source.reception  		{eval mbus_recv_rtp.source.reception $args}\n\
		rtp.source.active  		{eval mbus_recv_rtp.source.active $args}\n\
		rtp.source.inactive		{eval mbus_recv_rtp.source.inactive $args}\n\
		rtp.source.mute			{eval mbus_recv_rtp.source.mute $args}\n\
		rtp.source.gain			{eval mbus_recv_rtp.source.gain $args}\n\
		rtp.source.rtt			{eval mbus_recv_rtp.source.rtt $args}\n\
		rtp.source.app.site     {eval mbus_recv_rtp.source.app.site $args}\n\
		security.encryption.key 	{eval mbus_recv_security.encryption.key $args}\n\
		default				{bgerror \"Unknown mbus command $cmnd\"}\n\
	}\n\
}\n\
\n\
proc mbus_recv_mbus.waiting {condition} {\n\
	if {$condition == \"rat.ui.init\"} {\n\
		mbus_send \"U\" \"mbus.go\" [mbus_encode_str rat.ui.init]\n\
	}\n\
}\n\
\n\
proc mbus_recv_mbus.go {condition} {\n\
}\n\
\n\
proc mbus_recv_mbus.hello {} {\n\
	# Ignore...\n\
}\n\
\n\
proc change_freq {new_freq} {\n\
    global freq\n\
\n\
    if {$freq != $new_freq} {\n\
	set freq $new_freq\n\
	update_channels_displayed\n\
	update_codecs_displayed\n\
	reset_encodings\n\
    }\n\
}\n\
\n\
proc change_channels {new_channels} {\n\
    global ichannels\n\
    if {$ichannels != $new_channels} {\n\
	set ichannels $new_channels\n\
	update_codecs_displayed\n\
	reset_encodings\n\
    }\n\
}\n\
\n\
proc update_channels_displayed {} {\n\
    global freq channel_support\n\
\n\
    set m1 .prefs.pane.audio.dd.sampling.ch_in.mb.m\n\
    $m1 delete 0 last\n\
    set s [lsearch -glob $channel_support *$freq*]\n\
\n\
    foreach i [lrange [split [lindex $channel_support $s] \",\"] 1 2] {\n\
	 $m1 add command -label \"$i\" -command \"change_channels $i\"\n\
    }\n\
}\n\
\n\
proc mbus_recv_tool.rat.sampling.supported {arg} {\n\
    global freq channel_support\n\
\n\
    #clear away old state of channel support\n\
    if [info exists channel_support] {\n\
	unset channel_support\n\
    }\n\
\n\
    set freqs [list]\n\
    set channel_support [list]\n\
\n\
    .prefs.pane.audio.dd.sampling.freq.mb.m delete 0 last\n\
\n\
    set mode [split $arg]\n\
    foreach m $mode {\n\
	lappend channel_support $m\n\
	set support [split $m \",\"]\n\
	set f [lindex $support 0]\n\
	lappend freqs $f\n\
	.prefs.pane.audio.dd.sampling.freq.mb.m add command -label $f -command \"change_freq $f\"\n\
    }\n\
    set freq [lindex $freqs 0]\n\
    update_channels_displayed\n\
}\n\
\n\
# CODEC HANDLING ##############################################################\n\
\n\
set codecs {}\n\
set prencs  {}\n\
set secencs {}\n\
set layerencs {}\n\
set layerenc 1\n\
\n\
proc codec_get_name {nickname freq channels} {\n\
    global codecs codec_nick_name codec_rate codec_channels\n\
\n\
    foreach {c} $codecs {\n\
	if {$codec_nick_name($c)    == $nickname && \\\n\
		$codec_rate($c)     == $freq && \\\n\
		$codec_channels($c) == $channels} {\n\
	    return $c\n\
	}\n\
    }\n\
}\n\
\n\
proc codecs_loosely_matching {freq channels} {\n\
    global codecs codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc\n\
\n\
    set x {}\n\
\n\
    foreach {c} $codecs {\n\
	if {$codec_channels($c) == $channels && \\\n\
	$codec_rate($c) == $freq && \\\n\
	$codec_pt($c) != \"-\" } {\n\
	    lappend x $c\n\
	}\n\
    }\n\
\n\
    return $x\n\
}\n\
\n\
proc codecs_matching {freq channels blocksize} {\n\
    global codec_block_size\n\
    set codecs [codecs_loosely_matching $freq $channels]\n\
\n\
    set x {}\n\
\n\
    foreach {c} $codecs {\n\
	if {$codec_block_size($c) == $blocksize} {\n\
	    lappend x $c\n\
	}\n\
    }\n\
    return $x\n\
}\n\
\n\
proc mbus_recv_tool.rat.codecs.flush {args} {\n\
    global codecs\n\
\n\
    set codecs {}\n\
\n\
    set cvars [list codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc codec_caps codec_layers ]\n\
    foreach c $cvars {\n\
	global $c\n\
	upvar 0 $c v\n\
	if {[info exists v]} {\n\
	    unset v\n\
	}\n\
    }\n\
}\n\
\n\
proc mbus_recv_tool.rat.codecs.add {args} {\n\
    catch {\n\
	global codecs codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc codec_caps codec_layers\n\
\n\
	set name [lindex $args 1]\n\
	if {[lsearch $codecs $name] == -1} {\n\
	    lappend codecs $name\n\
	}\n\
	set codec_pt($name)         [lindex $args 0]\n\
	set codec_nick_name($name)  [lindex $args 2]\n\
	set codec_channels($name)   [lindex $args 3]\n\
	set codec_rate($name)       [lindex $args 4]\n\
	set codec_block_size($name) [lindex $args 5]\n\
	set codec_state_size($name) [lindex $args 6]\n\
	set codec_data_size($name)  [lindex $args 7]\n\
	set codec_desc($name)       [lindex $args 8]\n\
	set codec_caps($name)       [lindex $args 9]\n\
	set codec_layers($name)		[lindex $args 10]\n\
	set stackup \"\"\n\
    } details_error\n\
\n\
    if { $details_error != \"\" } {\n\
		bgerror \"Error: $details_error\"\n\
		destroy .\n\
		exit -1\n\
    }\n\
}\n\
\n\
proc update_primary_list {arg} {\n\
    # We now have a list of codecs which this RAT supports...\n\
    global prenc prencs\n\
\n\
    .prefs.pane.transmission.dd.pri.m.menu delete 0 last\n\
    set prencs {}\n\
\n\
    set codecs [split $arg]\n\
    foreach c $codecs {\n\
	.prefs.pane.transmission.dd.pri.m.menu    add command -label $c -command \"set prenc $c; update_codecs_displayed\"\n\
	lappend prencs $c\n\
    }\n\
\n\
    if {[lsearch $codecs $prenc] == -1} {\n\
	#primary is not on list\n\
	set prenc [lindex $codecs 0]\n\
    }\n\
}\n\
\n\
proc update_redundancy_list {arg} {\n\
    global secenc secencs\n\
\n\
    .prefs.pane.transmission.cc.red.fc.m.menu delete 0 last\n\
    set secencs {}\n\
\n\
    set codecs [split $arg]\n\
    foreach c $codecs {\n\
	.prefs.pane.transmission.cc.red.fc.m.menu add command -label $c -command \"set secenc \\\"$c\\\"\"\n\
	lappend secencs $c\n\
    }\n\
    if {[lsearch $codecs $secenc] == -1} {\n\
	#primary is not on list\n\
	set secenc [lindex $codecs 0]\n\
    }\n\
}\n\
\n\
proc update_layer_list {arg} {\n\
    global layerenc layerencs\n\
\n\
    .prefs.pane.transmission.cc.layer.fc.m.menu delete 0 last\n\
    set layerencs {}\n\
\n\
    for {set i 1} {$i <= $arg} {incr i} {\n\
	.prefs.pane.transmission.cc.layer.fc.m.menu add command -label $i -command \"set layerenc \\\"$i\\\"\"\n\
	lappend layerencs $i\n\
	}\n\
	if {$layerenc > $arg} {\n\
	#new codec doesn't support as many layers\n\
	set layerenc $arg\n\
	}\n\
}\n\
\n\
proc reset_encodings {} {\n\
    global prenc prencs secenc secencs layerenc layerencs\n\
    set prenc  [lindex $prencs 0]\n\
    set secenc [lindex $secencs 0]\n\
	set layerenc [lindex $layerencs 0]\n\
}\n\
\n\
proc update_codecs_displayed { } {\n\
    global freq ichannels codec_nick_name prenc codec_block_size codec_caps codec_layers\n\
\n\
    if {[string match $ichannels Mono]} {\n\
	set sample_channels 1\n\
    } else {\n\
	set sample_channels 2\n\
    }\n\
\n\
    set sample_rate [string trimright $freq -kHz]\n\
    set sample_rate [expr $sample_rate * 1000]\n\
    if {[expr $sample_rate % 11000] == 0} {\n\
	set sample_rate [expr $sample_rate * 11025 / 11000]\n\
    }\n\
\n\
    set long_names [codecs_loosely_matching $sample_rate $sample_channels]\n\
\n\
    set friendly_names {}\n\
    foreach {n} $long_names {\n\
	# only interested in codecs that can (e)ncode\n\
	if {[string first $codec_caps($n) ncode]} {\n\
	    lappend friendly_names $codec_nick_name($n)\n\
	}\n\
    }\n\
\n\
    update_primary_list $friendly_names\n\
\n\
    set long_name [codec_get_name $prenc $sample_rate $sample_channels]\n\
    set long_names [codecs_matching $sample_rate $sample_channels $codec_block_size($long_name)]\n\
\n\
    set friendly_names {}\n\
    set found 0\n\
    foreach {n} $long_names {\n\
	# Only display codecs of same or lower order as primary in primary list\n\
	if {$codec_nick_name($n) == $prenc} {\n\
	    set found 1\n\
	}\n\
	if {$found} {\n\
	    if {[string first $codec_caps($n) ncode]} {\n\
		lappend friendly_names $codec_nick_name($n)\n\
	    }\n\
	}\n\
    }\n\
\n\
    update_redundancy_list $friendly_names\n\
\n\
	# Assume that all codecs of one type support the same number of layers\n\
	foreach {n} $long_names {\n\
	if {$codec_nick_name($n) == $prenc} {\n\
	break\n\
	}\n\
	}\n\
	update_layer_list $codec_layers($n)\n\
}\n\
\n\
proc change_sampling { } {\n\
    update_channels_displayed\n\
    update_codecs_displayed\n\
}\n\
\n\
###############################################################################\n\
\n\
proc mbus_recv_tool.rat.converters.flush {} {\n\
    .prefs.pane.reception.r.ms.menu delete 0 last\n\
}\n\
\n\
proc mbus_recv_tool.rat.converters.add {arg} {\n\
    global convert_var\n\
    .prefs.pane.reception.r.ms.menu add command -label \"$arg\" -command \"set convert_var \\\"$arg\\\"\"\n\
}\n\
\n\
proc mbus_recv_tool.rat.converter {arg} {\n\
    global convert_var\n\
    set convert_var $arg\n\
}\n\
\n\
proc mbus_recv_tool.rat.repairs.flush {} {\n\
    .prefs.pane.reception.r.m.menu delete 0 last\n\
}\n\
\n\
\n\
proc mbus_recv_tool.rat.repairs.add {arg} {\n\
    global repair_var\n\
    .prefs.pane.reception.r.m.menu add command -label \"$arg\" -command \"set repair_var \\\"$arg\\\"\"\n\
}\n\
\n\
proc mbus_recv_audio_devices_flush {} {\n\
    .prefs.pane.audio.dd.device.mdev.menu delete 0 last\n\
}\n\
\n\
proc mbus_recv_audio_devices_add {arg} {\n\
    global audio_device\n\
\n\
    .prefs.pane.audio.dd.device.mdev.menu add command -label \"$arg\" -command \"set audio_device \\\"$arg\\\"\"\n\
\n\
    set len [string length \"$arg\"]\n\
    set curr [.prefs.pane.audio.dd.device.mdev cget -width]\n\
\n\
    if {$len > $curr} {\n\
	.prefs.pane.audio.dd.device.mdev configure -width $len\n\
    }\n\
}\n\
\n\
proc mbus_recv_audio_device {arg} {\n\
	global audio_device\n\
	set audio_device $arg\n\
}\n\
\n\
proc mbus_recv_tool.rat.powermeter {arg} {\n\
	global meter_var\n\
	set meter_var $arg\n\
}\n\
\n\
proc mbus_recv_tool.rat.agc {arg} {\n\
  global agc_var\n\
  set agc_var $arg\n\
}\n\
\n\
proc mbus_recv_security.encryption.key {new_key} {\n\
      global key_var key\n\
      set key     $new_key\n\
\n\
      if { [string length $key]!=0 } {\n\
	set key_var 1\n\
      } else {\n\
	set key_var 0\n\
      }\n\
      update_security_indicator\n\
}\n\
\n\
proc mbus_recv_tool.rat.format.in {arg} {\n\
    global freq ichannels\n\
#expect arg to be <sample_type>,<sample rate>,<mono/stereo>\n\
    set e [split $arg \",\"]\n\
\n\
    set freq      [lindex $e 1]\n\
    set ichannels [lindex $e 2]\n\
}\n\
\n\
proc mbus_recv_tool.rat.codec {arg} {\n\
  global prenc\n\
  set prenc $arg\n\
}\n\
\n\
proc mbus_recv_tool.rat.rate {arg} {\n\
    global upp\n\
    set upp $arg\n\
}\n\
\n\
proc mbus_recv_audio.channel.coding {args} {\n\
    global channel_var secenc red_off int_units int_gap prenc layerenc\n\
\n\
    set channel_var [lindex $args 0]\n\
\n\
    switch [string tolower $channel_var] {\n\
    	redundancy {\n\
		set secenc  [lindex $args 1]\n\
		set red_off [lindex $args 2]\n\
	}\n\
	interleaved {\n\
		set int_units [lindex $args 1]\n\
		set int_gap   [lindex $args 2]\n\
	}\n\
	layering {\n\
#		should we be playing with primary encoding?\n\
#		set prenc	  [lindex $args 1]\n\
		set layerenc  [lindex $args 2]\n\
	}\n\
    }\n\
}\n\
\n\
proc mbus_recv_audio.channel.repair {arg} {\n\
  global repair_var\n\
  set repair_var $arg\n\
}\n\
\n\
proc mbus_recv_audio.input.powermeter {level} {\n\
	bargraph::setHeight .r.c.tx.au.pow.bar [expr ($level * $bargraph::totalHeight) / 100]\n\
}\n\
\n\
proc mbus_recv_audio.output.powermeter {level} {\n\
	bargraph::setHeight .r.c.rx.au.pow.bar  [expr ($level * $bargraph::totalHeight) / 100]\n\
}\n\
\n\
proc mbus_recv_audio.input.gain {new_gain} {\n\
    global received gain\n\
    set received(gain) 1\n\
    set gain $new_gain\n\
}\n\
\n\
proc mbus_recv_audio.input.ports.flush {} {\n\
    global iports\n\
    set iports [list]\n\
}\n\
\n\
proc mbus_recv_audio.input.ports.add {port} {\n\
    global iports\n\
    lappend iports \"$port\"\n\
}\n\
\n\
proc mbus_recv_audio.input.port {port} {\n\
    global iport\n\
    set    iport $port\n\
}\n\
\n\
proc mbus_recv_audio.input.mute {val} {\n\
    global in_mute_var\n\
    set in_mute_var $val\n\
    if {$val} {\n\
        bargraph::disable .r.c.tx.au.pow.bar\n\
    } else {\n\
        bargraph::enable .r.c.tx.au.pow.bar\n\
    }\n\
}\n\
\n\
proc mbus_recv_audio.output.gain {new_gain} {\n\
    global received volume\n\
    set received(volume) 1\n\
    set volume $new_gain\n\
}\n\
\n\
proc mbus_recv_audio.output.port {port} {\n\
    global oport\n\
    set oport $port\n\
}\n\
\n\
proc mbus_recv_audio.output.ports.flush {} {\n\
    global oports\n\
    set oports [list]\n\
}\n\
\n\
proc mbus_recv_audio.output.ports.add {port} {\n\
    global oports\n\
    lappend oports \"$port\"\n\
}\n\
\n\
proc mbus_recv_audio.output.mute {val} {\n\
    global out_mute_var\n\
    set out_mute_var $val\n\
}\n\
\n\
proc mbus_recv_session.title {title} {\n\
    global session_title tool_name\n\
    set session_title \\\"$title\\\"\n\
    wm title . \"$tool_name: $title\"\n\
    wm deiconify .\n\
}\n\
\n\
proc mbus_recv_rtp.addr {addr rx_port tx_port ttl} {\n\
    global session_address group_addr g_rx_port g_tx_port g_ttl\n\
    set g_rx_port $rx_port\n\
    set g_tx_port $tx_port\n\
    set g_ttl $ttl\n\
    set group_addr $addr\n\
    set session_address \"$addr/$rx_port/$ttl\"\n\
}\n\
\n\
proc mbus_recv_tool.rat.lecture.mode {mode} {\n\
	global lecture_var\n\
	set lecture_var $mode\n\
}\n\
\n\
\n\
proc mbus_recv_audio.suppress.silence {mode} {\n\
    global silence_var\n\
\n\
#    if {$mode == 0} {\n\
#	set silence_var \"Off\"\n\
#    } else {\n\
#	set silence_var \"Automatic\"\n\
#    }\n\
#    puts \"audio.suppress.silence silence_var $silence_var\"\n\
}\n\
\n\
proc mbus_recv_tool.rat.silence {mode} {\n\
    global silence_var\n\
    switch -regexp $mode {\n\
	^[Oo].* { set silence_var Off }\n\
	^[Aa].* { set silence_var Automatic }\n\
	^[Mm].* { set silence_var Manual }\n\
    }\n\
}\n\
\n\
proc mbus_recv_tool.rat.silence.threshold {thresh} {\n\
    global manual_silence_thresh\n\
    set manual_silence_thresh $thresh\n\
}\n\
\n\
proc mbus_recv_rtp.ssrc {ssrc} {\n\
	global my_ssrc\n\
\n\
	set my_ssrc $ssrc\n\
	init_source $ssrc\n\
	ssrc_update $ssrc\n\
#        puts \"Got my_ssrc $ssrc\"\n\
}\n\
\n\
proc mbus_recv_rtp.source.exists {ssrc} {\n\
	init_source $ssrc\n\
	chart::add $ssrc\n\
	ssrc_update $ssrc\n\
}\n\
\n\
proc mbus_recv_rtp.source.cname {ssrc cname} {\n\
	global CNAME NAME SSRC\n\
	init_source $ssrc\n\
	set CNAME($ssrc) $cname\n\
	if {[string compare $NAME($ssrc) $SSRC($ssrc)] == 0} {\n\
		set NAME($ssrc) $cname\n\
	}\n\
	chart::setlabel $ssrc $NAME($ssrc)\n\
	ssrc_update $ssrc\n\
}\n\
\n\
proc mbus_recv_rtp.source.name {ssrc name} {\n\
	global NAME\n\
	init_source $ssrc\n\
	set NAME($ssrc) $name\n\
	chart::setlabel $ssrc $name\n\
	ssrc_update $ssrc\n\
}\n\
\n\
proc mbus_recv_rtp.source.email {ssrc email} {\n\
	global EMAIL\n\
	init_source $ssrc\n\
	set EMAIL($ssrc) $email\n\
}\n\
\n\
proc mbus_recv_rtp.source.phone {ssrc phone} {\n\
	global PHONE\n\
	init_source $ssrc\n\
	set PHONE($ssrc) $phone\n\
}\n\
\n\
proc mbus_recv_rtp.source.loc {ssrc loc} {\n\
	global LOC\n\
	init_source $ssrc\n\
	set LOC($ssrc) $loc\n\
}\n\
\n\
proc mbus_recv_rtp.source.priv {ssrc priv} {\n\
	global PRIV\n\
	init_source $ssrc\n\
	set PRIV($ssrc) $priv\n\
}\n\
\n\
proc mbus_recv_rtp.source.tool {ssrc tool} {\n\
	global TOOL my_ssrc tool_name\n\
	init_source $ssrc\n\
	set TOOL($ssrc) $tool\n\
	if {[info exists my_ssrc] && [string compare $ssrc $my_ssrc] == 0} {\n\
	    global tool_name\n\
	    # tool name looks like RAT x.x.x platform ....\n\
	    # lose the platform stuff\n\
	    set tool_frag [split $tool]\n\
	    set tool_name \"[lindex $tool_frag 0] [lindex $tool_frag 1]\"\n\
	}\n\
}\n\
\n\
proc mbus_recv_rtp.source.note {ssrc note} {\n\
	global NOTE\n\
	init_source $ssrc\n\
	set NOTE($ssrc) $note\n\
}\n\
\n\
\n\
proc mbus_recv_rtp.source.codec {ssrc codec} {\n\
	global CODEC\n\
	init_source $ssrc\n\
	set CODEC($ssrc) $codec\n\
}\n\
\n\
proc mbus_recv_rtp.source.gain {ssrc gain} {\n\
	global GAIN\n\
	init_source $ssrc\n\
	set GAIN($ssrc) $gain\n\
}\n\
\n\
proc mbus_recv_rtp.source.packet.duration {ssrc packet_duration} {\n\
	global DURATION\n\
	init_source $ssrc\n\
	set DURATION($ssrc) $packet_duration\n\
}\n\
\n\
proc mbus_recv_tool.rat.audio.buffered {ssrc buffered} {\n\
        global BUFFER_SIZE\n\
        init_source $ssrc\n\
        set BUFFER_SIZE($ssrc) $buffered\n\
}\n\
\n\
proc mbus_recv_tool.rat.audio.delay {ssrc len} {\n\
        global PLAYOUT_DELAY\n\
        init_source $ssrc\n\
        set PLAYOUT_DELAY($ssrc) $len\n\
}\n\
\n\
proc mbus_recv_tool.rat.audio.skew {ssrc skew} {\n\
        global SKEW\n\
        init_source $ssrc\n\
        set SKEW($ssrc) [format \"%.3f\" $skew]\n\
}\n\
\n\
proc mbus_recv_tool.rat.spike.events {ssrc events} {\n\
    global SPIKE_EVENTS\n\
    init_source $ssrc\n\
    set SPIKE_EVENTS($ssrc) $events\n\
}\n\
\n\
proc mbus_recv_tool.rat.spike.toged {ssrc toged} {\n\
    global SPIKE_TOGED\n\
    init_source $ssrc\n\
    set SPIKE_TOGED($ssrc) $toged\n\
}\n\
\n\
proc mbus_recv_rtp.source.rtt {ssrc rtt} {\n\
    global RTT\n\
    init_source $ssrc\n\
    set RTT($ssrc) $rtt\n\
}\n\
\n\
proc mbus_recv_audio.3d.enabled {mode} {\n\
	global 3d_audio_var\n\
	set 3d_audio_var $mode\n\
}\n\
\n\
proc mbus_recv_audio.3d.azimuth.min {min} {\n\
    global 3d_azimuth\n\
    set 3d_azimuth(min) $min\n\
}\n\
\n\
proc mbus_recv_audio.3d.azimuth.max {max} {\n\
    global 3d_azimuth\n\
    set 3d_azimuth(max) $max\n\
}\n\
\n\
proc mbus_recv_audio.3d.filter.types {args} {\n\
    global 3d_filters\n\
    set 3d_filters [split $args \",\"]\n\
}\n\
\n\
proc mbus_recv_audio.3d.filter.lengths {args} {\n\
    global 3d_filter_lengths\n\
    set 3d_filter_lengths [split $args \",\"]\n\
}\n\
\n\
proc mbus_recv_audio.3d.user.settings {args} {\n\
    global filter_type filter_length azimuth\n\
    set ssrc                 [lindex $args 0]\n\
    set filter_type($ssrc)   [lindex $args 1]\n\
    set filter_length($ssrc) [lindex $args 2]\n\
    set azimuth($ssrc)       [lindex $args 3]\n\
}\n\
\n\
proc mbus_recv_rtp.source.packet.loss {dest srce loss} {\n\
	global my_ssrc LOSS_FROM_ME LOSS_TO_ME HEARD_LOSS_FROM_ME HEARD_LOSS_TO_ME\n\
	init_source $srce\n\
	init_source $dest\n\
	chart::setvalue $srce $dest $loss\n\
	if {[info exists my_ssrc] && [string compare $dest $my_ssrc] == 0} {\n\
		set LOSS_TO_ME($srce) $loss\n\
    		set HEARD_LOSS_TO_ME($srce) 1\n\
	}\n\
	if {[info exists my_ssrc] && [string compare $srce $my_ssrc] == 0} {\n\
		set LOSS_FROM_ME($dest) $loss\n\
		set HEARD_LOSS_FROM_ME($dest) 1\n\
	}\n\
	ssrc_update $srce\n\
	ssrc_update $dest\n\
}\n\
\n\
proc mbus_recv_rtp.source.reception {ssrc packets_recv packets_lost packets_miso packets_dup jitter jit_tog} {\n\
	global PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER JIT_TOGED\n\
	init_source $ssrc\n\
	set PCKTS_RECV($ssrc) $packets_recv\n\
	set PCKTS_LOST($ssrc) $packets_lost\n\
	set PCKTS_MISO($ssrc) $packets_miso\n\
	set PCKTS_DUP($ssrc)  $packets_dup\n\
	set JITTER($ssrc) $jitter\n\
	set JIT_TOGED($ssrc) $jit_tog\n\
}\n\
\n\
proc mbus_recv_rtp.source.active {ssrc} {\n\
	global speaker_highlight\n\
	init_source $ssrc\n\
	ssrc_update $ssrc\n\
	[window_plist $ssrc] configure -background $speaker_highlight\n\
}\n\
\n\
proc mbus_recv_rtp.source.inactive {ssrc} {\n\
	init_source $ssrc\n\
	ssrc_update $ssrc\n\
	[window_plist $ssrc] configure -background [.l.t.list cget -bg]\n\
}\n\
\n\
proc mbus_recv_rtp.source.remove {ssrc} {\n\
    global loss_to_me_timer loss_from_me_timer num_ssrc\n\
    set did_remove 0\n\
\n\
    # Disable updating of loss diamonds. This has to be done before we destroy the\n\
    # window representing the participant, else the background update may try to\n\
    # access a window which has been destroyed...\n\
\n\
    catch {after cancel $loss_to_me_timer($ssrc)}\n\
    catch {after cancel $loss_from_me_timer($ssrc)}\n\
\n\
    set pvars [list CNAME NAME EMAIL LOC PHONE TOOL NOTE PRIV CODEC DURATION \\\n\
	    PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER BUFFER_SIZE \\\n\
	    PLAYOUT_DELAY LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED loss_to_me_timer \\\n\
	    loss_from_me_timer GAIN MUTE HEARD_LOSS_TO_ME HEARD_LOSS_FROM_ME  \\\n\
	    SKEW SPIKE_EVENTS SPIKE_TOGED RTT]\n\
\n\
    # safely delete array entries\n\
    foreach i $pvars {\n\
	global $i\n\
	upvar 0 $i v\n\
	if {[info exists v($ssrc)]} {\n\
	    unset v($ssrc)\n\
	    set did_remove 1\n\
	}\n\
    }\n\
\n\
    catch [destroy [window_plist $ssrc]]\n\
    chart::remove $ssrc\n\
\n\
    set stats_win [window_stats $ssrc]\n\
    if { [winfo exists $stats_win] } {\n\
	destroy $stats_win\n\
    }\n\
\n\
    if {$did_remove} {\n\
	incr num_ssrc -1\n\
    }\n\
}\n\
\n\
proc mbus_recv_rtp.source.mute {ssrc val} {\n\
	global iht MUTE\n\
	set MUTE($ssrc) $val\n\
	if {$val} {\n\
		[window_plist $ssrc] create line [expr $iht + 2] [expr $iht / 2] 500 [expr $iht / 2] -tags a -width 2.0 -fill gray95\n\
	} else {\n\
		catch [[window_plist $ssrc] delete a]\n\
	}\n\
}\n\
\n\
proc mbus_recv_rtp.source.app.site {ssrc val} {\n\
}\n\
\n\
proc mbus_recv_audio.file.play.ready {name} {\n\
    global play_file\n\
    set    play_file(name) $name\n\
\n\
    if {$play_file(state) != \"play\"} {\n\
	file_enable_play\n\
    }\n\
}\n\
\n\
proc mbus_recv_audio.file.play.alive {alive} {\n\
\n\
    global play_file\n\
\n\
    if {$alive} {\n\
	after 200 file_play_live\n\
    } else {\n\
	set play_file(state) end\n\
	file_enable_play\n\
    }\n\
}\n\
\n\
proc mbus_recv_audio.file.record.ready {name} {\n\
    global rec_file\n\
    set    rec_file(name) $name\n\
    if {$rec_file(state) != \"record\"} {\n\
	file_enable_record\n\
    }\n\
}\n\
\n\
proc mbus_recv_audio.file.record.alive {alive} {\n\
	global rec_file\n\
	if {$alive} {\n\
		after 200 file_rec_live\n\
	} else {\n\
		set rec_file(state) end\n\
		file_enable_record\n\
	}\n\
}\n\
\n\
proc mbus_recv_tool.rat.playout.limit {e} {\n\
    global limit_var\n\
    set limit_var $e\n\
}\n\
\n\
proc mbus_recv_tool.rat.playout.min {l} {\n\
    global min_var\n\
    set min_var $l\n\
}\n\
\n\
proc mbus_recv_tool.rat.playout.max {u} {\n\
    global max_var\n\
    set max_var $u\n\
}\n\
\n\
proc mbus_recv_tool.rat.echo.suppress {e} {\n\
    global echo_var\n\
    set echo_var $e\n\
}\n\
\n\
proc mbus_recv_tool.rat.loopback.gain {l} {\n\
    global audio_loop_var\n\
    set audio_loop_var $l\n\
}\n\
\n\
proc format_bps {bps} {\n\
    if {$bps < 999} {\n\
	set s \"b/s\"\n\
    } elseif {$bps < 999999} {\n\
	set s \"kb/s\"\n\
	set bps [expr $bps / 1000.0]\n\
    } else {\n\
	set s \"Mb/s\"\n\
	set bps [expr $bps / 1000000.0]\n\
    }\n\
    return [format \"%.1f %s\" $bps $s]\n\
}\n\
\n\
proc mbus_recv_tool.rat.bps.in {bps} {\n\
    global bps_in\n\
    set    bps_in [format_bps $bps]\n\
}\n\
\n\
proc mbus_recv_tool.rat.bps.out {bps} {\n\
    global bps_out\n\
    set    bps_out [format_bps $bps]\n\
}\n\
\n\
proc mbus_recv_mbus.quit {} {\n\
	destroy .\n\
}\n\
\n\
#############################################################################################################\n\
\n\
proc set_loss_to_me {ssrc loss} {\n\
	global prev_loss_to_me loss_to_me_timer\n\
\n\
	catch {after cancel $loss_to_me_timer($ssrc)}\n\
	set loss_to_me_timer($ssrc) [after 7500 catch \\\"[window_plist $ssrc] itemconfigure h -fill grey\\\"]\n\
\n\
	if {$loss < 5} {\n\
		catch [[window_plist $ssrc] itemconfigure m -fill green]\n\
	} elseif {$loss < 10} {\n\
		catch [[window_plist $ssrc] itemconfigure m -fill orange]\n\
	} elseif {$loss <= 100} {\n\
		catch [[window_plist $ssrc] itemconfigure m -fill red]\n\
	} else {\n\
		catch [[window_plist $ssrc] itemconfigure m -fill grey]\n\
	}\n\
}\n\
\n\
proc set_loss_from_me {ssrc loss} {\n\
	global prev_loss_from_me loss_from_me_timer\n\
\n\
	catch {after cancel $loss_from_me_timer($ssrc)}\n\
	set loss_from_me_timer($ssrc) [after 7500 catch \\\"[window_plist $ssrc] itemconfigure h -fill grey\\\"]\n\
\n\
	if {$loss < 5} {\n\
		catch [[window_plist $ssrc] itemconfigure h -fill green]\n\
	} elseif {$loss < 10} {\n\
		catch [[window_plist $ssrc] itemconfigure h -fill orange]\n\
	} elseif {$loss <= 100} {\n\
		catch [[window_plist $ssrc] itemconfigure h -fill red]\n\
	} else {\n\
		catch [[window_plist $ssrc] itemconfigure h -fill grey]\n\
	}\n\
}\n\
\n\
proc ssrc_update {ssrc} {\n\
	# This procedure updates the on-screen representation of\n\
	# a participant.\n\
	global NAME LOSS_TO_ME LOSS_FROM_ME HEARD_LOSS_FROM_ME HEARD_LOSS_TO_ME\n\
	global fw iht iwd my_ssrc\n\
\n\
	set cw [window_plist $ssrc]\n\
\n\
	if {[winfo exists $cw]} {\n\
		$cw itemconfigure t -text $NAME($ssrc)\n\
	} else {\n\
		# Add this participant to the list...\n\
		set thick 0\n\
		set l $thick\n\
		set h [expr $iht / 2 + $thick]\n\
		set f [expr $iht + $thick]\n\
		canvas $cw -width $iwd -height $f -highlightthickness $thick\n\
		$cw create text [expr $f + 2] $h -anchor w -text $NAME($ssrc) -fill black -tag t\n\
		$cw create polygon $l $h $h $l $h $f -outline black -fill grey -tag m\n\
		$cw create polygon $f $h $h $l $h $f -outline black -fill grey -tag h\n\
	        scale ${cw}.gain_scale_$ssrc -showvalue 0 -orient h -from -3 -to +3 -resolution 0.25 -command \"ssrc_set_gain $ssrc\"\n\
	        $cw create window [expr $iwd + 1] $h -anchor e -window $cw.gain_scale_$ssrc \n\
\n\
		bind $cw <Button-1>         \"toggle_stats \\\"$ssrc\\\"\"\n\
		bind $cw <Button-2>         \"toggle_mute $cw \\\"$ssrc\\\"\"\n\
		bind $cw <Control-Button-1> \"toggle_mute $cw \\\"$ssrc\\\"\"\n\
\n\
		if {[info exists my_ssrc] && ([string compare $ssrc $my_ssrc] == 0) && ([pack slaves $fw] != \"\")} {\n\
			pack $cw -before [lindex [pack slaves $fw] 0] -fill x\n\
		}\n\
		pack $cw -fill x\n\
		fix_scrollbar\n\
	}\n\
\n\
	if {[info exists HEARD_LOSS_TO_ME($ssrc)] && $HEARD_LOSS_TO_ME($ssrc) && [info exists my_ssrc] && ($ssrc != $my_ssrc)} {\n\
	    set_loss_to_me $ssrc $LOSS_TO_ME($ssrc)\n\
	}\n\
	if {[info exists HEARD_LOSS_FROM_ME($ssrc)] && $HEARD_LOSS_FROM_ME($ssrc) && [info exists my_ssrc] && ($ssrc != $my_ssrc)} {\n\
	    set_loss_from_me $ssrc $LOSS_FROM_ME($ssrc)\n\
	}\n\
}\n\
\n\
proc toggle {varname} {\n\
    upvar 1 $varname local\n\
    set local [expr !$local]\n\
}\n\
\n\
proc toggle_plist {} {\n\
	global plist_on\n\
	if {$plist_on} {\n\
		pack .l.t  -side top -fill both -expand 1 -padx 2\n\
	} else {\n\
		pack forget .l.t\n\
	}\n\
	update\n\
	wm deiconify .\n\
}\n\
\n\
proc toggle_status {} {\n\
	global status_on\n\
	if {$status_on} {\n\
                pack .s -before .st -side bottom -fill x -padx 2 -pady 0\n\
	} else {\n\
		pack forget .s\n\
	}\n\
	update\n\
	wm deiconify .\n\
}\n\
\n\
proc toggle_mute {cw ssrc} {\n\
	global iht\n\
	if {[$cw gettags a] == \"\"} {\n\
		mbus_send \"R\" \"rtp.source.mute\" \"[mbus_encode_str $ssrc] 1\"\n\
	} else {\n\
		mbus_send \"R\" \"rtp.source.mute\" \"[mbus_encode_str $ssrc] 0\"\n\
	}\n\
}\n\
\n\
proc send_gain_and_mute {ssrc} {\n\
	global GAIN MUTE\n\
	mbus_send \"R\" \"rtp.source.gain\" \"[mbus_encode_str $ssrc] $GAIN($ssrc)\"\n\
	mbus_send \"R\" \"rtp.source.mute\" \"[mbus_encode_str $ssrc] $MUTE($ssrc)\"\n\
}\n\
\n\
proc send_tone_cmd {} {\n\
    global tonegen\n\
\n\
    if {$tonegen} {\n\
	# tone generator turned on (Note A, gain 16767 / 32768)\n\
	mbus_send \"R\" \"tool.rat.tone.start\" \"440 16767\"\n\
    } else {\n\
	mbus_send \"R\" \"tool.rat.tone.stop\" \"\"\n\
    }\n\
}\n\
\n\
proc fix_scrollbar {} {\n\
	global iht iwd fw\n\
\n\
	set ch [expr $iht * ([llength [pack slaves $fw]] + 2)]\n\
	set bh [winfo height .l.t.scr]\n\
	if {$ch > $bh} {set h $ch} else {set h $bh}\n\
	.l.t.list configure -scrollregion \"0.0 0.0 $iwd $h\"\n\
}\n\
\n\
proc info_timer {} {\n\
	global cancel_info_timer\n\
	if {$cancel_info_timer == 1} {\n\
		set cancel_info_timer 0\n\
	} else {\n\
		update_rec_info\n\
		after 1000 info_timer\n\
	}\n\
}\n\
\n\
proc stats_add_field {widget label watchVar} {\n\
    global statsfont\n\
    frame $widget\n\
    label $widget.l -text $label -font $statsfont -anchor w\n\
    label $widget.w -textvariable $watchVar -font $statsfont\n\
    pack $widget -side top -fill x -anchor n\n\
    pack $widget.l -side left  -fill x -expand 1\n\
    pack $widget.w -side right\n\
}\n\
\n\
proc ssrc_set_gain {ssrc gain} {\n\
    global GAIN\n\
    set    GAIN($ssrc) [format \"%.2f \" [expr pow (2, $gain)]]\n\
    send_gain_and_mute $ssrc\n\
}\n\
\n\
set 3d_azimuth(min) 0\n\
set 3d_azimuth(max) 0\n\
set 3d_filters        [list \"Not Available\"]\n\
set 3d_filter_lengths [list \"0\"]\n\
\n\
proc toggle_stats {ssrc} {\n\
    global statsfont\n\
    set win [window_stats $ssrc]\n\
    if {[winfo exists $win]} {\n\
	destroy $win\n\
    } else {\n\
	global stats_pane\n\
	# Window does not exist so create it\n\
	toplevel $win\n\
	frame $win.mf\n\
	pack $win.mf -padx 0 -pady 0\n\
	label $win.mf.l -text \"Category:\"\n\
\n\
	menubutton $win.mf.mb -menu $win.mf.mb.menu -indicatoron 1 -textvariable stats_pane($win) -relief raised -width 16\n\
	pack $win.mf.l $win.mf.mb -side left\n\
	menu $win.mf.mb.menu -tearoff 0\n\
	$win.mf.mb.menu add command -label \"Personal Details\" -command \"set_pane stats_pane($win) $win.df \\\"Personal Details\\\"\"\n\
	$win.mf.mb.menu add command -label \"Playout\"          -command \"set_pane stats_pane($win) $win.df Playout\"\n\
	$win.mf.mb.menu add command -label \"Decoder\"          -command \"set_pane stats_pane($win) $win.df Decoder\"\n\
	$win.mf.mb.menu add command -label \"Audio\"            -command \"set_pane stats_pane($win) $win.df Audio\"\n\
	$win.mf.mb.menu add command -label \"3D Positioning\"   -command \"set_pane stats_pane($win) $win.df \\\"3D Positioning\\\"\"\n\
\n\
	set stats_pane($win) \"Personal Details\"\n\
	frame $win.df\n\
	frame $win.df.personal -relief groove -bd 2\n\
	pack  $win.df -side top -anchor n -expand 1 -fill both\n\
	pack $win.df.personal -expand 1 -fill both\n\
\n\
	global NAME EMAIL PHONE LOC NOTE PRIV CNAME TOOL SSRC\n\
	stats_add_field $win.df.personal.1 \"Name: \"     NAME($ssrc)\n\
	stats_add_field $win.df.personal.2 \"Email: \"    EMAIL($ssrc)\n\
	stats_add_field $win.df.personal.3 \"Phone: \"    PHONE($ssrc)\n\
	stats_add_field $win.df.personal.4 \"Location: \" LOC($ssrc)\n\
	stats_add_field $win.df.personal.5 \"Note: \"     NOTE($ssrc)\n\
	stats_add_field $win.df.personal.6 \"Priv: \"     PRIV($ssrc)\n\
	stats_add_field $win.df.personal.7 \"Tool: \"     TOOL($ssrc)\n\
	stats_add_field $win.df.personal.8 \"CNAME: \"    CNAME($ssrc)\n\
	stats_add_field $win.df.personal.9 \"SSRC: \"     SSRC($ssrc)\n\
\n\
	frame $win.df.playout -relief groove -bd 2\n\
	global BUFFER_SIZE PLAYOUT_DELAY JITTER JIT_TOGED SKEW\n\
	stats_add_field $win.df.playout.1 \"Actual Playout (ms): \"     BUFFER_SIZE($ssrc)\n\
	stats_add_field $win.df.playout.2 \"Target Playout (ms): \"     PLAYOUT_DELAY($ssrc)\n\
	stats_add_field $win.df.playout.3 \"Arrival Jitter (ms): \"     JITTER($ssrc)\n\
	stats_add_field $win.df.playout.4 \"Packets Dropped (jitter):\" JIT_TOGED($ssrc)\n\
	stats_add_field $win.df.playout.5 \"Spike Events:\"             SPIKE_EVENTS($ssrc)\n\
	stats_add_field $win.df.playout.6 \"Packets Dropped (spike):\"  SPIKE_TOGED($ssrc)\n\
	stats_add_field $win.df.playout.7 \"Relative Clock Rate:\"      SKEW($ssrc)\n\
\n\
	frame $win.df.decoder -relief groove -bd 2\n\
	global CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO \\\n\
	       PCKTS_DUP LOSS_FROM_ME LOSS_TO_ME\n\
	stats_add_field $win.df.decoder.1 \"Audio encoding: \"         CODEC($ssrc)\n\
	stats_add_field $win.df.decoder.2 \"Packet duration (ms): \"   DURATION($ssrc)\n\
	stats_add_field $win.df.decoder.3 \"Loss from me (%): \"       LOSS_FROM_ME($ssrc)\n\
	stats_add_field $win.df.decoder.4 \"Loss to me (%): \"         LOSS_TO_ME($ssrc)\n\
	stats_add_field $win.df.decoder.5 \"Packets received: \"       PCKTS_RECV($ssrc)\n\
	stats_add_field $win.df.decoder.6 \"Packets lost: \"           PCKTS_LOST($ssrc)\n\
	stats_add_field $win.df.decoder.7 \"Packets misordered: \"     PCKTS_MISO($ssrc)\n\
	stats_add_field $win.df.decoder.8 \"Packets duplicated: \"     PCKTS_DUP($ssrc)\n\
	stats_add_field $win.df.decoder.9 \"Round Trip Time (ms):\"    RTT($ssrc)\n\
\n\
# Audio settings\n\
	global GAIN MUTE\n\
	frame $win.df.audio -relief groove -bd 2\n\
	label $win.df.audio.advice -text \"The signal from the participant can\\nbe scaled and muted with the controls below.\"\n\
	pack  $win.df.audio.advice\n\
\n\
	checkbutton $win.df.audio.mute -text \"Mute\" -variable MUTE($ssrc) -command \"send_gain_and_mute $ssrc\"\n\
	pack $win.df.audio.mute\n\
\n\
	frame $win.df.audio.opts\n\
	pack  $win.df.audio.opts -side top\n\
	label $win.df.audio.opts.title -text \"Gain\"\n\
	scale $win.df.audio.opts.gain_scale -showvalue 0 -orient h -from -3 -to +3 -resolution 0.25 -command \"ssrc_set_gain $ssrc\"\n\
	label $win.df.audio.opts.gain_text -textvariable GAIN($ssrc) -width 4\n\
	pack  $win.df.audio.opts.title $win.df.audio.opts.gain_scale $win.df.audio.opts.gain_text -side left\n\
\n\
	$win.df.audio.opts.gain_scale set [expr log10($GAIN($ssrc)) / log10(2)]\n\
\n\
	button $win.df.audio.default -text \"Default\" -command \"set MUTE($ssrc) 0; $win.df.audio.opts.gain_scale set 0.0; send_gain_and_mute $ssrc\"\n\
	pack   $win.df.audio.default -side bottom  -anchor e -padx 2 -pady 2\n\
\n\
# 3D settings\n\
	# Trigger engine to send details for this participant\n\
	mbus_send \"R\" \"audio.3d.user.settings.request\" [mbus_encode_str $ssrc]\n\
\n\
	frame $win.df.3d -relief groove -bd 2\n\
	label $win.df.3d.advice -text \"These options allow the rendering of the\\nparticipant to be altered when 3D\\nrendering is enabled.\"\n\
	checkbutton $win.df.3d.ext -text \"3D Audio Rendering\" -variable 3d_audio_var\n\
	pack $win.df.3d.advice\n\
	pack $win.df.3d.ext\n\
\n\
	frame $win.df.3d.opts\n\
	pack $win.df.3d.opts -side top\n\
\n\
	frame $win.df.3d.opts.filters\n\
	label $win.df.3d.opts.filters.l -text \"Filter Type:\"\n\
	pack $win.df.3d.opts.filters.l -side left -fill x -expand 1 -anchor n\n\
	global 3d_filters 3d_filter_lengths\n\
\n\
	global filter_type\n\
	set filter_type($ssrc) [lindex $3d_filters 0]\n\
\n\
	set cnt 0\n\
	foreach i $3d_filters {\n\
	    radiobutton $win.df.3d.opts.filters.$cnt \\\n\
		    -value \"$i\" -variable filter_type($ssrc) \\\n\
		    -text \"$i\"\n\
		pack $win.df.3d.opts.filters.$cnt -side top -anchor w\n\
	    incr cnt\n\
	}\n\
\n\
	frame $win.df.3d.opts.lengths\n\
	label $win.df.3d.opts.lengths.l -text \"Length:\"\n\
	pack $win.df.3d.opts.lengths.l -side left -fill x -expand 1 -anchor n\n\
\n\
	global filter_length\n\
	set filter_length($ssrc) [lindex $3d_filter_lengths 0]\n\
\n\
	set cnt 0\n\
	foreach i $3d_filter_lengths {\n\
	    radiobutton $win.df.3d.opts.lengths.$cnt \\\n\
		    -value \"$i\" -variable filter_length($ssrc) \\\n\
		    -text \"$i\"\n\
	    pack $win.df.3d.opts.lengths.$cnt -side top -anchor w\n\
	    incr cnt\n\
	}\n\
	pack $win.df.3d.opts.filters -side left -expand 1 -anchor n -fill x\n\
	pack $win.df.3d.opts.lengths -side right -expand 1 -anchor n -fill x\n\
\n\
	global 3d_azimuth azimuth\n\
	frame $win.df.3d.azi\n\
	label $win.df.3d.azi.lab -text \"Azimuth:\"\n\
	scale $win.df.3d.azi.sca -from $3d_azimuth(min) -to $3d_azimuth(max) \\\n\
		-orient horizontal -variable azimuth($ssrc)\n\
	pack  $win.df.3d.azi\n\
	pack $win.df.3d.azi.lab $win.df.3d.azi.sca -side left\n\
\n\
	button $win.df.3d.apply -text \"Apply\" -command \"3d_send_parameters $ssrc\"\n\
	pack   $win.df.3d.apply -side bottom  -anchor e -padx 2 -pady 2\n\
\n\
# Window Magic\n\
	wm title $win \"Participant $NAME($ssrc)\"\n\
#	wm resizable $win 1 1\n\
	wm protocol  $win WM_DELETE_WINDOW \"destroy $win; 3d_delete_parameters $ssrc\"\n\
	constrain_window $win $statsfont 36 22\n\
    }\n\
}\n\
\n\
proc 3d_send_parameters {ssrc} {\n\
    global azimuth filter_type filter_length 3d_audio_var\n\
\n\
    mbus_send \"R\" \"audio.3d.enabled\"   $3d_audio_var\n\
    mbus_send \"R\" \"audio.3d.user.settings\" \"[mbus_encode_str $ssrc] [mbus_encode_str $filter_type($ssrc)] $filter_length($ssrc) $azimuth($ssrc)\"\n\
}\n\
\n\
proc 3d_delete_parameters {ssrc} {\n\
    global filter_type filter_length azimuth\n\
\n\
# None of these should ever fail, but you can't be too defensive...\n\
    catch {\n\
	unset filter_type($ssrc)\n\
	unset filter_length($ssrc)\n\
	unset azimuth($ssrc)\n\
    }\n\
}\n\
\n\
proc do_quit {} {\n\
	catch {\n\
		profile off pdat\n\
		profrep pdat cpu\n\
	}\n\
	destroy .\n\
}\n\
\n\
proc toggle_chart {} {\n\
	global matrix_on\n\
	if {$matrix_on} {\n\
		chart::show\n\
	} else {\n\
		chart::hide\n\
	}\n\
}\n\
\n\
proc update_security_indicator {} {\n\
 global key_var key icons\n\
\n\
 if {$key_var == 1 && [string length $key]!=0 } {\n\
    .r.f.f.padlock configure -image $icons(padlock) -bg lightyellow\n\
    .r.f.f.pad configure -bg lightyellow\n\
    .r.f.f.addr configure -bg lightyellow\n\
  } else {\n\
    .r.f.f.padlock configure -image \"\" -bg gray70\n\
    .r.f.f.pad configure -bg gray70\n\
    .r.f.f.addr configure -bg gray70 \n\
  }\n\
}\n\
\n\
proc valid_rtp_addr {addr rx_port tx_port ttl} {\n\
  global group_addr g_rx_port g_tx_port g_ttl\n\
\n\
  if { [string match \\[0-9\\]* $rx_port] && [string match \\[0-9\\]* $tx_port] } {\n\
      if { $rx_port > 1023 && $rx_port < 65535 && $tx_port > 1023 && $tx_port < 65535 } {\n\
        set addrl [split $addr .]\n\
        if { [llength $addrl] == 4 } {\n\
          foreach i $addrl {\n\
            if {[string match \\[0-9\\]* $i]} {\n\
              if { $i < 0 || $i > 255 } { \n\
				return \"\\\"$group_addr\\\" $g_rx_port $g_tx_port $g_ttl\" \n\
			  }\n\
            } else {\n\
				return \"\\\"$group_addr\\\" $g_rx_port $g_tx_port $g_ttl\"\n\
			}            \n\
		  }\n\
          if {[string match \\[0-9\\]* $ttl]} {\n\
              if { $ttl > -1 && $ttl < 256 } { \n\
		         return \"\\\"$addr\\\" $rx_port $tx_port $ttl\"\n\
	          }\n\
	      }\n\
       }\n\
     }\n\
  }\n\
  return \"\\\"$group_addr\\\" $g_rx_port $g_tx_port $g_ttl\"\n\
}\n\
\n\
proc change_rtp_addr {l_session_address} {\n\
  global in_mute_var my_ssrc\n\
  global group_addr g_rx_port g_tx_port g_ttl\n\
\n\
  focus .\n\
  \n\
  # Return if session_address unchanged\n\
  if { $l_session_address == \"$group_addr/$g_rx_port/$g_ttl\" } { return }\n\
\n\
  # Create new rtp_addr string to signal to mbus_engine\n\
  set rtp_addrl [ split $l_session_address /]\n\
  switch  [llength $rtp_addrl]  {\n\
    1  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] $g_rx_port $g_tx_port $g_ttl]}\n\
    2  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] [lindex $rtp_addrl 1] [lindex $rtp_addrl 1] $g_ttl]}\n\
    3  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] [lindex $rtp_addrl 1] [lindex $rtp_addrl 1] [lindex $rtp_addrl 2]] }\n\
    4  { set rtp_addr [valid_rtp_addr [lindex $rtp_addrl 0] [lindex $rtp_addrl 1] [lindex $rtp_addrl 2] [lindex $rtp_addrl 3]] }\n\
    default \n\
	   { set rtp_addr \"\\\"$group_addr\\\" $g_rx_port $g_tx_port $g_ttl\"}\n\
  }\n\
  # required \"rtp.addr\": \"\\\"addr\\\" src_port dst_port ttl\" \n\
  # Check if new address is valid\n\
  # e.g rtp.addr \"224.1.2.3\" 1234    1234    16\n\
  #               mcast      rx_port tx_port ttl\n\
  mbus_send \"R\" \"rtp.addr\" \"$rtp_addr\"\n\
}\n\
\n\
\n\
# Initialise RAT MAIN window\n\
frame .r\n\
frame .l\n\
frame .l.t -relief groove -bd 2\n\
if {$::tk_version < 8.5} {\n\
    scrollbar .l.t.scr -relief flat -highlightthickness 0 -command \".l.t.list yview\"\n\
} else {\n\
    ttk::style map TScrollbar -background [list disabled [.l.t cget -bg]]\n\
\n\
    ttk::scrollbar .l.t.scr -command \".l.t.list yview\"\n\
}\n\
canvas .l.t.list -highlightthickness 0 -bd 0 -relief flat -width $iwd -height 120 -yscrollcommand \".l.t.scr set\" -yscrollincrement $iht\n\
frame .l.t.list.f -highlightthickness 0 -bd 0\n\
.l.t.list create window 0 0 -anchor nw -window .l.t.list.f\n\
\n\
set icons(file_open) [image create photo -format gif  -data { R0lGODlhEgASAPIAAAAAAICAAMDAwPj8APj8+AAAAAAAAAAAACH5BAEAAAIALAAAAAASABIAAAM7KLrc/jAKQCUDC2N7t6JeA2YDMYDoA5hsWYZk28LfjN4b4AJB7/ue1elHDOl4RKAImQzQcDeOdEp1JAAAO///}]\n\
\n\
set icons(file_save) [image create photo -data { R0lGODlhEgASAPEAAAAAAICAAMDAwAAAACH5BAEAAAIALAAAAAASABIAAAI3lI+pywYPY0QgAHbvqVpBamHhNnqlwIkdeoJrZUlPcML0jde0DOnxxHrtJA6frLhC8YiNpnNRAAA7////}]\n\
\n\
set icons(padlock) [image create photo -format gif -data { R0lGODlhCgAMAMZaAJRpAJRtAJRtCJRxAJxtAJxxAJx1AKV1AJx9AKV5AKV9AKV9CKWCAK2GAK2GCK2KALWGAHuKjISGjK2OAISKjISKlLWOAISKnISOlLWSCIySnIyWpZSapZyelKWqlKWmraWqnK26xrW+vbW+xr2+tbXDxr3DtefLIb3Dzu/PKcbLtcbPrcbL1s7Xpe/bQtbXpc7T1u/fQt7frffnSvfnUt7jrf/rWvfrc97j597j7+frtf/va//zY/f3a/fzhPf3e//ze+/vvf/zhP/3c+fv7//7c+/3vffzvf/7hPf7nP/3nPf3vff3xv/7nPf/pf//nP/7tf//pf/7vf//tf//xv//zv//1v//3v//5///7////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////yH5BAEKAH8ALAAAAAAKAAwAAAd0gEuCKiIhJEGCgiY5REQ4HzpHSyssIxURIzAgiBwoFC01EiUaNUwbGB0yRh4XGi8ZFhMQDQwKCggLFlBZvFhYV1IGD0lWTUI+UlVKBQ9AVD88NEpTNwMNNlM7MzE9TS4EDClOUU9IRUMnAA62CQcDAQEAAoEAOw==}]\n\
\n\
\n\
frame .r.f -relief groove -bd 2 -width $iwd\n\
frame .r.f.f -relief sunk\n\
label .r.f.f.padlock -image \"\" -relief flat -bg gray70\n\
label .r.f.f.pad -text \"\" -relief flat -bg gray70\n\
label .r.f.addrlab -text \"Addr/port/ttl: \" -relief flat \n\
entry .r.f.f.addr  -bd 1 -textvariable session_address -relief flat -bg gray70\n\
bind .r.f.f.addr <KeyPress-Return> { change_rtp_addr $session_address }\n\
\n\
frame       .st -bd 0\n\
\n\
checkbutton .st.file -image $icons(file_save) -indicatoron 0 -variable files_on  -command file_show\n\
checkbutton .st.recp -bd 1 -bitmap reception -indicatoron 0 -variable matrix_on -command toggle_chart\n\
checkbutton .st.help -bitmap balloon   -indicatoron 0 -variable help_on \n\
\n\
button      .st.opts  -text \"Options...\" -pady 2 -command {wm deiconify .prefs; update_user_panel; raise .prefs}\n\
button      .st.about -text \"About...\"   -pady 2 -command {jiggle_credits; wm deiconify .about}\n\
button      .st.quit  -text \"Quit\"       -pady 2 -command do_quit\n\
\n\
frame .r.c -bd 0\n\
frame .r.c.rx -relief groove -bd 2\n\
frame .r.c.tx -relief groove -bd 2\n\
\n\
frame .s -relief groove -bd 2 -width $iwd\n\
label .s.status -text \"Audio Status: \" -relief flat -font $compfont\n\
label .s.statustxt -text \"wwwww\" -relief flat -textvariable audio_device -font $compfont\n\
\n\
pack .st -side bottom -fill x -padx 2 -pady 0\n\
pack .st.file .st.recp .st.help -side left -anchor e -padx 2 -pady 2\n\
\n\
pack .st.quit .st.about .st.opts -side right -anchor w -padx 2 -pady 2\n\
\n\
pack .s -before .st -side bottom -fill x -padx 2 -pady 0\n\
pack .s.status -side left -anchor e -padx 2 -pady 2\n\
pack .s.statustxt -side left -anchor e -padx 2 -pady 2\n\
\n\
# hide status bar as Default\n\
pack forget .s\n\
\n\
pack .r -side top -fill x -padx 2\n\
pack .r.f -side top -fill x -padx 0 -pady 2\n\
pack .r.f.addrlab .r.f.f -side left \n\
pack .r.f.f.addr .r.f.f.pad .r.f.f.padlock -side left -pady 2 -anchor n -fill x\n\
pack .r.c -side top -fill x -expand 1\n\
pack .r.c.rx -side left -fill x -expand 1\n\
pack .r.c.tx -side left -fill x -expand 1\n\
\n\
pack .l -side top -fill both -expand 1\n\
#pack .l.f.title -side top -pady 2 -anchor w -fill x\n\
pack .l.t  -side top -fill both -expand 1 -padx 2\n\
if {$::tk_version < 8.5} {\n\
    pack .l.t.scr -side left -fill y\n\
} else {\n\
    pack .l.t.scr -side right -fill y\n\
}\n\
pack .l.t.list -side left -fill both -expand 1\n\
bind .l.t.list <Configure> {fix_scrollbar}\n\
\n\
# Device output controls\n\
set out_mute_var 0\n\
frame .r.c.rx.net\n\
frame .r.c.rx.au\n\
\n\
pack .r.c.rx.net -side top -anchor n -fill both\n\
pack .r.c.rx.au -side bottom -expand 1 -fill both\n\
\n\
checkbutton .r.c.rx.net.on -highlightthickness 0 -text \"Listen\" -onvalue 0 -offvalue 1 -variable out_mute_var -command {output_mute $out_mute_var} -font $compfont -width 7 -anchor w -padx 4\n\
label       .r.c.rx.net.bw -textv bps_in -font $compfont -anchor e -width 8\n\
pack .r.c.rx.net.on -side left\n\
pack .r.c.rx.net.bw -side right -fill x -expand 1\n\
\n\
frame  .r.c.rx.au.port -bd 0\n\
button .r.c.rx.au.port.l0 -highlightthickness 0 -command {incr_port oport oports -1} -font $compfont -bitmap right -relief flat\n\
label  .r.c.rx.au.port.l1 -highlightthickness 0 -textv oport -font $compfont -width 10\n\
button .r.c.rx.au.port.l2 -highlightthickness 0 -command {incr_port oport oports +1} -font $compfont -bitmap  left -relief flat\n\
label  .r.c.rx.au.port.vlabel -text \"Vol\"\n\
label  .r.c.rx.au.port.vval   -textv volume -width 3 -justify right\n\
pack   .r.c.rx.au.port -side top -fill x -expand 1\n\
\n\
pack   .r.c.rx.au.port.l2 -side left -fill y\n\
pack   .r.c.rx.au.port.l1 -side left -fill x -expand 0\n\
pack  .r.c.rx.au.port.l0 -side left -fill y\n\
\n\
\n\
pack   .r.c.rx.au.port.vval .r.c.rx.au.port.vlabel -side right\n\
\n\
frame  .r.c.rx.au.pow -relief sunk -bd 2\n\
bargraph::create .r.c.rx.au.pow.bar\n\
scale .r.c.rx.au.pow.sli -highlightthickness 0 -from 0 -to 99 -command set_vol -orient horizontal -showvalue false -width 8 -variable volume -resolution 4 -bd 0 -troughcolor black -sliderl 16\n\
pack .r.c.rx.au.pow .r.c.rx.au.pow.bar .r.c.rx.au.pow.sli -side top -fill both -expand 1\n\
\n\
# Device input controls\n\
set in_mute_var 1\n\
\n\
frame .r.c.tx.net\n\
frame .r.c.tx.au\n\
\n\
pack .r.c.tx.net -side top -anchor n  -fill both\n\
pack .r.c.tx.au -side bottom -expand 1 -fill both\n\
\n\
checkbutton .r.c.tx.net.on -highlightthickness 0 -text \"Talk\" -onvalue 0 -offvalue 1 -variable in_mute_var -command {input_mute $in_mute_var} -font $compfont -width 7 -anchor w -padx 4\n\
label       .r.c.tx.net.bw -textv bps_out -font $compfont -anchor e -width 8\n\
pack .r.c.tx.net.on -side left\n\
pack .r.c.tx.net.bw -side right -fill x -expand 1\n\
\n\
frame  .r.c.tx.au.port -bd 0\n\
button .r.c.tx.au.port.l0 -highlightthickness 0 -command {incr_port iport iports -1} -font $compfont -bitmap right -relief flat\n\
label  .r.c.tx.au.port.l1 -highlightthickness 0 -textv iport -font $compfont -width 10\n\
button .r.c.tx.au.port.l2 -highlightthickness 0 -command {incr_port iport iports +1} -font $compfont -bitmap  left -relief flat\n\
label  .r.c.tx.au.port.vlabel -text \"Gain\"\n\
label  .r.c.tx.au.port.vval   -textv gain -width 3 -justify right\n\
pack   .r.c.tx.au.port -side top -fill x -expand 1\n\
pack   .r.c.tx.au.port.l2 -side left -fill y\n\
pack   .r.c.tx.au.port.l1 -side left -fill x -expand 0\n\
pack   .r.c.tx.au.port.l0 -side left -fill y\n\
pack   .r.c.tx.au.port.vval .r.c.tx.au.port.vlabel -side right\n\
\n\
frame  .r.c.tx.au.pow -relief sunk -bd 2\n\
bargraph::create .r.c.tx.au.pow.bar\n\
scale .r.c.tx.au.pow.sli -highlightthickness 0 -from 0 -to 99 -command set_gain -orient horizontal -showvalue false -width 8 -variable gain -resolution 4 -bd 0 -sliderl 16 -troughco black\n\
pack .r.c.tx.au.pow .r.c.tx.au.pow.bar .r.c.tx.au.pow.sli -side top -fill both -expand 1\n\
\n\
bind all <ButtonPress-3>   {toggle in_mute_var; input_mute $in_mute_var}\n\
bind all <ButtonRelease-3> {toggle in_mute_var; input_mute $in_mute_var}\n\
bind all <q>               {+if {[winfo class %W] != \"Entry\"} {do_quit}}\n\
\n\
# Override default tk behaviour\n\
wm protocol . WM_DELETE_WINDOW do_quit\n\
wm title    . Initializing...\n\
\n\
if {$win32 == 0} {\n\
	wm iconbitmap . rat_small\n\
}\n\
#wm resizable . 0 1\n\
if ([info exists geometry]) {\n\
        wm geometry . $geometry\n\
}\n\
\n\
proc averageCharacterWidth {font} {\n\
    set sample \"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\"\n\
    set slen [string length $sample]\n\
    set wpc  [expr [font measure $font $sample] / $slen + 1]\n\
    return $wpc\n\
}\n\
\n\
# constrain_window - window, font, characters wide, characters high\n\
proc constrain_window {w font cW cH} {\n\
\n\
    catch {\n\
            set wpc [averageCharacterWidth $font]\n\
            set hpc [font metrics $font -ascent]\n\
\n\
        # Calculate dimensions\n\
            set width [expr $cW * $wpc]\n\
            set height [expr $cH * $hpc]\n\
            wm geometry $w [format \"%sx%s\" $width $height]\n\
            set dummy \"\"\n\
    } err\n\
    if {$err != \"\"} {\n\
        bgerror \"Error: $err\"\n\
    }\n\
}\n\
\n\
proc tk_optionCmdMenu {w varName firstValue args} {\n\
    upvar #0 $varName var\n\
\n\
    if ![info exists var] {\n\
        set var $firstValue\n\
    }\n\
    menubutton $w -textvariable $varName -indicatoron 1 -menu $w.menu \\\n\
            -relief raised -bd 2 -highlightthickness 2 -anchor c\n\
\n\
    menu $w.menu -tearoff 0\n\
    $w.menu add command -label $firstValue -command \"set $varName \\\"$firstValue\\\"\"\n\
    foreach i $args {\n\
        $w.menu add command -label $i -command \"set $varName \\\"$i\\\"\"\n\
    }\n\
    return $w.menu\n\
}\n\
\n\
\n\
###############################################################################\n\
# Preferences Panel\n\
#\n\
\n\
set prefs_pane \"Personal\"\n\
toplevel .prefs\n\
wm title .prefs \"Preferences\"\n\
wm resizable .prefs 0 0\n\
wm withdraw  .prefs\n\
\n\
frame .prefs.m\n\
pack .prefs.m -side top -fill x -expand 0 -padx 2 -pady 2\n\
frame .prefs.m.f\n\
pack .prefs.m.f -padx 0 -pady 0\n\
label .prefs.m.f.t -text \"Category: \"\n\
pack .prefs.m.f.t -pady 2 -side left\n\
menubutton .prefs.m.f.m -menu .prefs.m.f.m.menu -indicatoron 1 -textvariable prefs_pane -relief raised -width 14\n\
pack .prefs.m.f.m -side top\n\
menu .prefs.m.f.m.menu -tearoff 0\n\
.prefs.m.f.m.menu add command -label \"Personal\"     -command {set_pane prefs_pane .prefs.pane \"Personal\"}\n\
.prefs.m.f.m.menu add command -label \"Transmission\" -command {set_pane prefs_pane .prefs.pane \"Transmission\"; update_codecs_displayed}\n\
.prefs.m.f.m.menu add command -label \"Reception\"    -command {set_pane prefs_pane .prefs.pane \"Reception\"}\n\
.prefs.m.f.m.menu add command -label \"Audio\"        -command {set_pane prefs_pane .prefs.pane \"Audio\"}\n\
.prefs.m.f.m.menu add command -label \"Codecs\"        -command {set_pane prefs_pane .prefs.pane \"Codecs\"; codecs_panel_fill}\n\
.prefs.m.f.m.menu add command -label \"Security\"     -command {set_pane prefs_pane .prefs.pane \"Security\"}\n\
.prefs.m.f.m.menu add command -label \"Interface\"    -command {set_pane prefs_pane .prefs.pane \"Interface\"}\n\
\n\
frame  .prefs.buttons\n\
pack   .prefs.buttons       -side bottom -fill x\n\
button .prefs.buttons.bye   -text \"Cancel\" -command {sync_ui_to_engine; wm withdraw .prefs}\n\
button .prefs.buttons.apply -text \"Apply\" -command {wm withdraw .prefs; sync_engine_to_ui}\n\
pack   .prefs.buttons.bye .prefs.buttons.apply -side right -padx 2 -pady 2\n\
\n\
wm protocol .prefs WM_DELETE_WINDOW {sync_ui_to_engine; wm withdraw .prefs}\n\
\n\
frame .prefs.pane -relief groove -bd 2\n\
pack  .prefs.pane -side left -fill both -expand 1 -padx 4 -pady 2\n\
\n\
# setup width of prefs panel\n\
constrain_window .prefs $infofont 56 30\n\
\n\
# Personal Info Pane ##########################################################\n\
set i .prefs.pane.personal\n\
frame $i\n\
pack $i -fill both -expand 1 -pady 2 -padx 2\n\
\n\
frame $i.a\n\
frame $i.a.f -rel fl\n\
pack $i.a -side top -fill both -expand 1\n\
pack $i.a.f -side left -fill x -expand 1\n\
\n\
frame $i.a.f.f -rel fl\n\
pack $i.a.f.f\n\
\n\
label $i.a.f.f.l -width 40 -height 2 -text \"The personal details below are conveyed\\nto the other conference participants.\" -justify left -anchor w\n\
pack $i.a.f.f.l -side top -anchor w -fill x\n\
\n\
frame $i.a.f.f.lbls\n\
frame $i.a.f.f.ents\n\
pack  $i.a.f.f.lbls -side left -fill y\n\
pack  $i.a.f.f.ents -side right\n\
\n\
label $i.a.f.f.lbls.1 -text \"Name:\"     -anchor w\n\
label $i.a.f.f.lbls.2 -text \"Email:\"    -anchor w\n\
label $i.a.f.f.lbls.3 -text \"Phone:\"    -anchor w\n\
label $i.a.f.f.lbls.4 -text \"Location:\" -anchor w\n\
label $i.a.f.f.lbls.5 -text \"Note:\" -anchor w\n\
pack $i.a.f.f.lbls.1 $i.a.f.f.lbls.2 $i.a.f.f.lbls.3 $i.a.f.f.lbls.4 $i.a.f.f.lbls.5 -fill x -anchor w -side top\n\
\n\
entry $i.a.f.f.ents.name  -width 28 -highlightthickness 0 -textvariable rtcp_name\n\
entry $i.a.f.f.ents.email -width 28 -highlightthickness 0 -textvariable rtcp_email\n\
entry $i.a.f.f.ents.phone -width 28 -highlightthickness 0 -textvariable rtcp_phone\n\
entry $i.a.f.f.ents.loc   -width 28 -highlightthickness 0 -textvariable rtcp_loc\n\
entry $i.a.f.f.ents.note  -width 28 -highlightthickness 0 -textvariable rtcp_note\n\
pack $i.a.f.f.ents.name $i.a.f.f.ents.email $i.a.f.f.ents.phone $i.a.f.f.ents.loc $i.a.f.f.ents.note -anchor n -expand 0\n\
\n\
proc update_user_panel {} {\n\
    global my_ssrc NAME EMAIL PHONE LOC NOTE PRIV\n\
    global rtcp_name rtcp_email rtcp_phone rtcp_loc rtcp_note\n\
    if {[info exists my_ssrc]} {\n\
        set rtcp_name  $NAME($my_ssrc)\n\
        set rtcp_email $EMAIL($my_ssrc)\n\
        set rtcp_phone $PHONE($my_ssrc)\n\
        set rtcp_loc   $LOC($my_ssrc)\n\
        set rtcp_note  $NOTE($my_ssrc)\n\
    }\n\
}\n\
\n\
# Transmission Pane ###########################################################\n\
set i .prefs.pane.transmission\n\
frame $i\n\
frame $i.dd\n\
frame $i.cc\n\
frame $i.cc.van\n\
frame $i.cc.red\n\
frame $i.cc.layer\n\
frame $i.cc.int\n\
label $i.intro -text \"This panel allows you to select codecs for transmission.  The choice\\nof codecs available depends on the sampling rate and channels\\nin the audio panel.\"\n\
frame $i.title1-f -relief raised\n\
label $i.title1-f.l -text \"Audio Encoding\"\n\
pack $i.intro $i.title1-f $i.title1-f.l $i.dd -side top -fill x\n\
\n\
#pack $i.dd -fill x -side top -anchor n\n\
\n\
frame $i.title2-f -relief raised\n\
label $i.title2-f.l -text \"Channel Coding Options\"\n\
pack $i.title2-f $i.title2-f.l -fill x -side top\n\
pack $i.cc -fill x -anchor w -pady 1\n\
\n\
pack $i.cc.van $i.cc.red $i.cc.layer -fill x -anchor w -pady 0\n\
# interleaving panel $i.cc.int not packed since interleaving isn't support in this release\n\
frame $i.dd.units\n\
frame $i.dd.pri\n\
\n\
pack $i.dd.units $i.dd.pri -side right -fill x\n\
\n\
label $i.dd.pri.l -text \"Encoding:\"\n\
menubutton $i.dd.pri.m -menu $i.dd.pri.m.menu -indicatoron 1 -textvariable prenc -relief raised -width 13\n\
pack $i.dd.pri.l $i.dd.pri.m -side top\n\
# fill in codecs\n\
menu $i.dd.pri.m.menu -tearoff 0\n\
\n\
label $i.dd.units.l -text \"Units:\"\n\
tk_optionCmdMenu $i.dd.units.m upp 1 2 4 8 16 32 64\n\
$i.dd.units.m configure -width 13 -highlightthickness 0 -bd 1\n\
pack $i.dd.units.l $i.dd.units.m -side top -fill x\n\
\n\
radiobutton $i.cc.van.rb -text \"No Loss Protection\" -justify right -value none        -variable channel_var\n\
radiobutton $i.cc.red.rb -text \"Redundancy\"         -justify right -value redundancy  -variable channel_var\n\
radiobutton $i.cc.layer.rb -text \"Layering\"			-justify right -value layering    -variable channel_var\n\
radiobutton $i.cc.int.rb -text \"Interleaving\"       -justify right -value interleaved -variable channel_var -state disabled\n\
pack $i.cc.van.rb $i.cc.red.rb $i.cc.layer.rb $i.cc.int.rb -side left -anchor nw -padx 2\n\
\n\
frame $i.cc.red.fc\n\
label $i.cc.red.fc.l -text \"Encoding:\"\n\
menubutton $i.cc.red.fc.m -textvariable secenc -indicatoron 1 -menu $i.cc.red.fc.m.menu -relief raised -width 13\n\
menu $i.cc.red.fc.m.menu -tearoff 0\n\
\n\
frame $i.cc.red.u\n\
label $i.cc.red.u.l -text \"Offset in Pkts:\"\n\
tk_optionCmdMenu $i.cc.red.u.m red_off \"1\" \"2\" \"4\" \"8\"\n\
$i.cc.red.u.m configure -width 13 -highlightthickness 0 -bd 1\n\
pack $i.cc.red.u -side right -anchor e -fill y\n\
pack $i.cc.red.u.l $i.cc.red.u.m -fill x\n\
pack $i.cc.red.fc -side right\n\
pack $i.cc.red.fc.l $i.cc.red.fc.m\n\
\n\
frame $i.cc.layer.fc\n\
label $i.cc.layer.fc.l -text \"Layers:\"\n\
menubutton $i.cc.layer.fc.m -textvariable layerenc -indicatoron 1 -menu $i.cc.layer.fc.m.menu -relief raised -width 13\n\
menu $i.cc.layer.fc.m.menu -tearoff 0\n\
pack $i.cc.layer.fc -side right\n\
pack $i.cc.layer.fc.l $i.cc.layer.fc.m\n\
\n\
frame $i.cc.int.zz\n\
label $i.cc.int.zz.l -text \"Units:\"\n\
tk_optionCmdMenu $i.cc.int.zz.m int_units 2 4 6 8\n\
$i.cc.int.zz.m configure -width 13 -highlightthickness 0 -bd 1 -state disabled\n\
\n\
frame $i.cc.int.fc\n\
label $i.cc.int.fc.l -text \"Separation:\"\n\
tk_optionCmdMenu $i.cc.int.fc.m int_gap 2 4 6 8\n\
$i.cc.int.fc.m configure -width 13 -highlightthickness 0 -bd 1 -state disabled\n\
\n\
pack $i.cc.int.fc $i.cc.int.zz -side right\n\
pack $i.cc.int.fc.l $i.cc.int.fc.m -fill x -expand 1\n\
pack $i.cc.int.zz.l $i.cc.int.zz.m -fill x -expand 1\n\
\n\
# Reception Pane ##############################################################\n\
set i .prefs.pane.reception\n\
frame $i\n\
frame $i.r -rel f\n\
frame $i.o -rel f\n\
frame $i.c -rel f\n\
pack $i.r -side top -fill x -pady 0 -ipady 1\n\
pack $i.o -side top -fill both  -pady 1\n\
pack $i.c -side top -fill both  -pady 1 -expand 1\n\
label $i.r.l -text \"Repair Scheme:\"\n\
tk_optionCmdMenu $i.r.m repair_var {}\n\
\n\
label $i.r.ls -text \"Sample Rate Conversion\"\n\
tk_optionCmdMenu $i.r.ms convert_var {}\n\
\n\
$i.r.m  configure -width 20 -bd 1\n\
$i.r.ms configure -width 20 -bd 1\n\
pack $i.r.l $i.r.m $i.r.ls $i.r.ms -side top\n\
\n\
frame $i.o.f\n\
checkbutton $i.o.f.cb -text \"Limit Playout Delay\" -variable limit_var\n\
frame $i.o.f.fl\n\
label $i.o.f.fl.l1 -text \"Minimum Delay (ms)\"\n\
scale $i.o.f.fl.scmin -orient horizontal -from 0 -to 1000    -variable min_var -font $smallfont\n\
frame $i.o.f.fr\n\
label $i.o.f.fr.l2 -text \"Maximum Delay (ms)\"\n\
scale $i.o.f.fr.scmax -orient horizontal -from 1000 -to 2000 -variable max_var -font $smallfont\n\
pack $i.o.f\n\
pack $i.o.f.cb -side top -fill x\n\
pack $i.o.f.fl $i.o.f.fr -side left\n\
pack $i.o.f.fl.l1 $i.o.f.fl.scmin $i.o.f.fr.l2 $i.o.f.fr.scmax -side top -fill x -expand 1\n\
\n\
frame $i.c.f\n\
frame $i.c.f.f\n\
checkbutton $i.c.f.f.lec -text \"Lecture Mode\"       -variable lecture_var\n\
checkbutton $i.c.f.f.ext -text \"3D Audio Rendering\" -variable 3d_audio_var\n\
\n\
pack $i.c.f -fill x -side left -expand 1\n\
pack $i.c.f.f\n\
pack $i.c.f.f.lec -side top  -anchor w\n\
pack $i.c.f.f.ext -side top  -anchor w\n\
\n\
# Audio #######################################################################\n\
set i .prefs.pane.audio\n\
frame $i\n\
frame $i.dd -rel fl\n\
pack $i.dd -fill both -expand 1 -anchor w -pady 1\n\
\n\
label $i.dd.title -height 2 -width 40 -text \"This panel allows for the selection of alternate audio devices\\nand the configuring of device related options.\" -justify left\n\
pack $i.dd.title -fill x\n\
\n\
frame $i.dd.device\n\
pack $i.dd.device -side top\n\
\n\
label $i.dd.device.l -text \"Audio Device:\"\n\
pack  $i.dd.device.l -side top -fill x\n\
menubutton $i.dd.device.mdev -menu $i.dd.device.mdev.menu -indicatoron 1 \\\n\
                                -textvariable audio_device -relief raised -width 5\n\
pack $i.dd.device.mdev -fill x -expand 1\n\
menu $i.dd.device.mdev.menu -tearoff 0\n\
\n\
frame $i.dd.sampling\n\
pack  $i.dd.sampling\n\
\n\
frame $i.dd.sampling.freq\n\
frame $i.dd.sampling.ch_in\n\
pack $i.dd.sampling.freq $i.dd.sampling.ch_in -side left -fill x\n\
\n\
label $i.dd.sampling.freq.l   -text \"Sample Rate:   \"\n\
label $i.dd.sampling.ch_in.l  -text \"Channels:\"\n\
pack $i.dd.sampling.freq.l $i.dd.sampling.ch_in.l -fill x\n\
\n\
menubutton $i.dd.sampling.freq.mb -menu $i.dd.sampling.freq.mb.m -indicatoron 1 \\\n\
                                  -textvariable freq -relief raised\n\
pack $i.dd.sampling.freq.mb -side left -fill x -expand 1\n\
menu $i.dd.sampling.freq.mb.m\n\
\n\
menubutton $i.dd.sampling.ch_in.mb -menu $i.dd.sampling.ch_in.mb.m -indicatoron 1 \\\n\
                                  -textvariable ichannels -relief raised\n\
pack $i.dd.sampling.ch_in.mb -side left -fill x -expand 1\n\
menu $i.dd.sampling.ch_in.mb.m\n\
\n\
frame $i.dd.cks\n\
pack $i.dd.cks -fill both -expand 1\n\
frame $i.dd.cks.f\n\
frame $i.dd.cks.f.f\n\
frame $i.dd.cks.f.f.silence\n\
frame $i.dd.cks.f.f.other\n\
\n\
frame $i.dd.cks.f.f.silence.upper\n\
label $i.dd.cks.f.f.silence.upper.title    -text \"Silence Suppression:\"\n\
radiobutton $i.dd.cks.f.f.silence.upper.r0 -text \"Off\"       -value \"Off\" -variable silence_var       -command send_silence_params\n\
radiobutton $i.dd.cks.f.f.silence.upper.r1 -text \"Automatic\" -value \"Automatic\" -variable silence_var -command send_silence_params\n\
radiobutton $i.dd.cks.f.f.silence.upper.r2 -text \"Manual\"    -value \"Manual\" -variable silence_var    -command send_silence_params\n\
\n\
frame $i.dd.cks.f.f.silence.lower\n\
scale $i.dd.cks.f.f.silence.lower.s -from 1 -to 500 -orient h -showvalue 0 -variable manual_silence_thresh -command send_silence_params\n\
label $i.dd.cks.f.f.silence.lower.ind -textvar manual_silence_thresh -width 5\n\
label $i.dd.cks.f.f.silence.lower.title -text \"Manual Silence Threshold:\"\n\
pack $i.dd.cks.f.f.silence.lower.title  -side top\n\
pack $i.dd.cks.f.f.silence.lower.s $i.dd.cks.f.f.silence.lower.ind -side left\n\
\n\
pack $i.dd.cks.f.f.silence.upper.title -side top -fill x\n\
pack $i.dd.cks.f.f.silence.upper.r0 $i.dd.cks.f.f.silence.upper.r1 $i.dd.cks.f.f.silence.upper.r2 -side left\n\
\n\
label $i.dd.cks.f.f.other.title -text \"Additional Audio Options:\"\n\
checkbutton $i.dd.cks.f.f.other.agc      -text \"Automatic Gain Control\" -variable agc_var\n\
checkbutton $i.dd.cks.f.f.other.loop     -text \"Audio Loopback\"         -variable audio_loop_var\n\
checkbutton $i.dd.cks.f.f.other.suppress -text \"Echo Suppression\"       -variable echo_var\n\
checkbutton $i.dd.cks.f.f.other.tone     -text \"Tone Test\"              -variable tonegen        -command send_tone_cmd\n\
pack $i.dd.cks.f -fill x -side top -expand 1\n\
pack $i.dd.cks.f.f\n\
pack $i.dd.cks.f.f.silence -side left -anchor n\n\
pack $i.dd.cks.f.f.silence.upper -side top -anchor n\n\
pack $i.dd.cks.f.f.silence.lower -side bottom -anchor n\n\
pack $i.dd.cks.f.f.other   -side right -anchor n\n\
pack $i.dd.cks.f.f.other.title -side top\n\
pack $i.dd.cks.f.f.other.agc $i.dd.cks.f.f.other.loop $i.dd.cks.f.f.other.suppress $i.dd.cks.f.f.other.tone -side top -anchor w\n\
\n\
# Codecs pane #################################################################\n\
set i .prefs.pane.codecs\n\
frame $i\n\
\n\
frame $i.of -rel fl\n\
pack  $i.of -fill both -expand 1 -anchor w -pady 1 -side top\n\
\n\
message $i.of.l -width 30c -justify left -text \"This panel shows available codecs and allows RTP payload re-mapping.\"\n\
pack $i.of.l -side top -fill x\n\
\n\
frame   $i.of.codecs\n\
\n\
pack    $i.of.codecs -side left -padx 2 -fill y\n\
frame   $i.of.codecs.l -rel raised\n\
label   $i.of.codecs.l.l -text \"Codec\"\n\
listbox $i.of.codecs.lb -width 20 -yscrollcommand \"$i.of.codecs.scroll set\" -relief fl -bg white\n\
if {$::tk_version < 8.5} {\n\
    scrollbar $i.of.codecs.scroll -command \"$i.of.codecs.lb yview\" -rel fl\n\
\n\
    pack    $i.of.codecs.l $i.of.codecs.l.l -side top -fill x\n\
    pack    $i.of.codecs.scroll $i.of.codecs.lb -side left -fill both\n\
} else {\n\
    ttk::scrollbar $i.of.codecs.scroll -command \"$i.of.codecs.lb yview\"\n\
\n\
    pack    $i.of.codecs.l $i.of.codecs.l.l -side top -fill x\n\
    pack    $i.of.codecs.scroll $i.of.codecs.lb -side right -fill both\n\
}\n\
\n\
frame   $i.of.details -bd 0\n\
pack    $i.of.details -side left -fill both -expand 1 -anchor n\n\
\n\
frame $i.of.details.upper\n\
pack $i.of.details.upper -fill x\n\
\n\
frame $i.of.details.desc\n\
pack $i.of.details.desc -side top -fill x\n\
\n\
frame $i.of.details.pt\n\
pack $i.of.details.pt -side bottom -fill x -anchor s\n\
label $i.of.details.pt.l -anchor w -text \"RTP payload:\"\n\
pack  $i.of.details.pt.l -side left -anchor w\n\
\n\
entry $i.of.details.pt.e -width 4\n\
pack  $i.of.details.pt.e -side left -padx 4\n\
\n\
button $i.of.details.pt.b -text \"Map Codec\" -command map_codec\n\
pack  $i.of.details.pt.b -side right -padx 4\n\
\n\
frame $i.of.details.upper.l0 -relief raised\n\
label $i.of.details.upper.l0.l -text \"Details\"\n\
pack $i.of.details.upper.l0 $i.of.details.upper.l0.l -side top -fill x -expand 1\n\
\n\
frame $i.of.details.upper.l\n\
pack $i.of.details.upper.l -side left\n\
label $i.of.details.upper.l.0 -text \"Short name:\"  -anchor w\n\
label $i.of.details.upper.l.1 -text \"Sample Rate (Hz):\" -anchor w\n\
label $i.of.details.upper.l.2 -text \"Channels:\"    -anchor w\n\
label $i.of.details.upper.l.3 -text \"Bitrate (kbps):\"     -anchor w\n\
label $i.of.details.upper.l.4 -text \"RTP Payload:\" -anchor w\n\
label $i.of.details.upper.l.5 -text \"Capability:\" -anchor w\n\
label $i.of.details.upper.l.6 -text \"Layers:\" -anchor w\n\
\n\
for {set idx 0} {$idx < 7} {incr idx} {\n\
    pack $i.of.details.upper.l.$idx -side top -fill x\n\
}\n\
\n\
frame $i.of.details.upper.r\n\
pack $i.of.details.upper.r -side left -fill x -expand 1\n\
label $i.of.details.upper.r.0 -anchor w\n\
label $i.of.details.upper.r.1 -anchor w\n\
label $i.of.details.upper.r.2 -anchor w\n\
label $i.of.details.upper.r.3 -anchor w\n\
label $i.of.details.upper.r.4 -anchor w\n\
label $i.of.details.upper.r.5 -anchor w\n\
label $i.of.details.upper.r.6 -anchor w\n\
\n\
for {set idx 0} {$idx < 7} {incr idx} {\n\
    pack $i.of.details.upper.r.$idx -side top -fill x\n\
}\n\
\n\
set descw [expr [averageCharacterWidth $infofont] * 30]\n\
label $i.of.details.desc.l -text \"Description:\" -anchor w -wraplength $descw -justify left\n\
pack $i.of.details.desc.l -side left -fill x\n\
unset descw\n\
\n\
bind $i.of.codecs.lb <1> {\n\
    codecs_panel_select [%W index @%x,%y]\n\
}\n\
\n\
bind $i.of.codecs.lb <ButtonRelease-1> {\n\
    codecs_panel_select [%W index @%x,%y]\n\
}\n\
proc codecs_panel_fill {} {\n\
    global codecs\n\
\n\
    .prefs.pane.codecs.of.codecs.lb delete 0 end\n\
\n\
    foreach {c} $codecs {\n\
	.prefs.pane.codecs.of.codecs.lb insert end $c\n\
    }\n\
}\n\
\n\
set last_selected_codec -1\n\
\n\
proc codecs_panel_select { idx } {\n\
    global codecs codec_nick_name codec_rate codec_channels codec_pt codec_block_size codec_data_size codec_desc codec_caps codec_layers\n\
    global last_selected_codec\n\
\n\
    set last_selected_codec $idx\n\
\n\
    set codec [lindex $codecs $idx]\n\
    set root  .prefs.pane.codecs.of.details.upper.r\n\
    $root.0 configure -text $codec_nick_name($codec)\n\
    $root.1 configure -text $codec_rate($codec)\n\
    $root.2 configure -text $codec_channels($codec)\n\
\n\
    set fps [expr $codec_rate($codec) * 2 * $codec_channels($codec) / $codec_block_size($codec) ]\n\
    set kbps [expr 8 * $fps * $codec_data_size($codec) / 1000.0]\n\
    $root.3 configure -text [format \"%.1f\" $kbps]\n\
\n\
    $root.4 configure -text $codec_pt($codec)\n\
    $root.5 configure -text $codec_caps($codec)\n\
    $root.6 configure -text $codec_layers($codec)\n\
\n\
    .prefs.pane.codecs.of.details.desc.l configure -text \"Description: $codec_desc($codec)\"\n\
\n\
}\n\
\n\
proc map_codec {} {\n\
    global codecs last_selected_codec\n\
\n\
    set idx $last_selected_codec\n\
\n\
    if {$last_selected_codec == -1} {\n\
	return\n\
    }\n\
\n\
    set pt [.prefs.pane.codecs.of.details.pt.e get]\n\
    .prefs.pane.codecs.of.details.pt.e delete 0 end\n\
\n\
    set ptnot [string trim $pt 1234567890]\n\
    if {$ptnot != \"\"} {\n\
	return\n\
    }\n\
\n\
    set codec [lindex $codecs $idx]\n\
\n\
    mbus_send \"R\" \"tool.rat.payload.set\" \"[mbus_encode_str $codec] $pt\"\n\
    after 1000 codecs_panel_select $idx\n\
}\n\
\n\
# Security Pane ###############################################################\n\
set i .prefs.pane.security\n\
frame $i\n\
frame $i.a -rel fl\n\
frame $i.a.f\n\
frame $i.a.f.f\n\
label $i.a.f.f.l -anchor w -justify left -text \"Your communication can be secured with\\nDES or AES encryption.  Only conference participants\\nwith the same key can receive audio data when\\nencryption is enabled.\"\n\
pack $i.a.f.f.l\n\
pack $i.a -side top -fill both -expand 1\n\
label $i.a.f.f.lbl -text \"Key:\"\n\
entry $i.a.f.f.e -width 28 -textvariable key\n\
checkbutton $i.a.f.f.cb -text \"Enabled\" -variable key_var -command update_security_indicator\n\
pack $i.a.f -fill x -side left -expand 1\n\
pack $i.a.f.f\n\
pack $i.a.f.f.lbl $i.a.f.f.e $i.a.f.f.cb -side left -pady 4 -padx 2 -fill x\n\
\n\
# Interface Pane ##############################################################\n\
set i .prefs.pane.interface\n\
frame $i\n\
frame $i.a -rel fl\n\
frame $i.a.f\n\
frame $i.a.f.f\n\
label $i.a.f.f.l -anchor w -justify left -text \"The following features may be\\ndisabled to conserve processing\\npower.\"\n\
pack $i.a -side top -fill both -expand 1\n\
pack $i.a.f -fill x -side left -expand 1\n\
checkbutton $i.a.f.f.power   -text \"Powermeters active\"       -variable meter_var\n\
checkbutton $i.a.f.f.balloon -text \"Balloon help\"             -variable help_on\n\
checkbutton $i.a.f.f.matrix  -text \"Reception quality matrix\" -variable matrix_on -command toggle_chart\n\
checkbutton $i.a.f.f.plist   -text \"Participant list\"         -variable plist_on  -command toggle_plist\n\
checkbutton $i.a.f.f.fwin    -text \"File Control Window\"      -variable files_on  -command file_show\n\
checkbutton $i.a.f.f.status  -text \"Status bar\"               -variable status_on  -command toggle_status\n\
pack $i.a.f.f $i.a.f.f.l\n\
pack $i.a.f.f.power $i.a.f.f.balloon $i.a.f.f.matrix $i.a.f.f.plist $i.a.f.f.fwin $i.a.f.f.status -side top -anchor w\n\
\n\
proc set_pane {p base desc} {\n\
    upvar 1 $p pane\n\
    set tpane [string tolower [lindex [split $pane] 0]]\n\
    pack forget $base.$tpane\n\
    set tpane [string tolower [lindex [split $desc] 0]]\n\
    pack $base.$tpane -fill both -expand 1 -padx 2 -pady 2\n\
    set pane $desc\n\
}\n\
\n\
# Initialise \"About...\" toplevel window\n\
toplevel   .about\n\
frame      .about.rim -relief groove -bd 2\n\
frame      .about.m\n\
frame      .about.rim.d\n\
pack .about.m -fill x\n\
pack .about.rim -padx 4 -pady 2 -side top -fill both -expand 1\n\
pack .about.rim.d -side top -fill both -expand 1\n\
\n\
frame .about.m.f\n\
label .about.m.f.l -text \"Category:\"\n\
menubutton .about.m.f.mb -menu .about.m.f.mb.menu -indicatoron 1 -textvariable about_pane -relief raised -width 10\n\
menu .about.m.f.mb.menu -tearoff 0\n\
.about.m.f.mb.menu add command -label \"Credits\"   -command {set_pane about_pane .about.rim.d \"Credits\"   }\n\
.about.m.f.mb.menu add command -label \"Feedback\"  -command {set_pane about_pane .about.rim.d \"Feedback\"  }\n\
.about.m.f.mb.menu add command -label \"Copyright\" -command {set_pane about_pane .about.rim.d \"Copyright\" }\n\
\n\
pack .about.m.f\n\
pack .about.m.f.l .about.m.f.mb -side left\n\
\n\
frame     .about.rim.d.copyright\n\
frame     .about.rim.d.copyright.f -relief flat\n\
frame     .about.rim.d.copyright.f.f\n\
text      .about.rim.d.copyright.f.f.blurb -height 14 -yscrollcommand \".about.rim.d.copyright.f.f.scroll set\" -relief flat -bg white\n\
if {$::tk_version < 8.5} {\n\
    scrollbar .about.rim.d.copyright.f.f.scroll -command \".about.rim.d.copyright.f.f.blurb yview\" -relief flat\n\
} else {\n\
    ttk::scrollbar .about.rim.d.copyright.f.f.scroll -command \".about.rim.d.copyright.f.f.blurb yview\"\n\
}\n\
pack      .about.rim.d.copyright.f -expand 1 -fill both\n\
pack      .about.rim.d.copyright.f.f -expand 1 -fill both\n\
if {$::tk_version < 8.5} {\n\
    pack  .about.rim.d.copyright.f.f.scroll -side left -fill y -expand 1\n\
} else {\n\
    pack  .about.rim.d.copyright.f.f.scroll -side right -fill y -expand 1\n\
}\n\
pack      .about.rim.d.copyright.f.f.blurb -side left -fill y -expand 1\n\
\n\
frame     .about.rim.d.credits\n\
frame     .about.rim.d.credits.f -relief flat\n\
frame     .about.rim.d.credits.f.f\n\
pack      .about.rim.d.credits.f -fill both -expand 1\n\
pack      .about.rim.d.credits.f.f -side left -fill x -expand 1\n\
label     .about.rim.d.credits.f.f.1                  -text \"The Robust-Audio Tool was developed in the Department of\\nComputer Science, University College London.\\n\\nProject Supervision:\"\n\
label     .about.rim.d.credits.f.f.2 -foreground blue -text Good\n\
label     .about.rim.d.credits.f.f.3                  -text \"Development Team:\"\n\
label     .about.rim.d.credits.f.f.4 -foreground blue -text Bad\n\
label     .about.rim.d.credits.f.f.5                  -text \"Additional Contributions:\"\n\
label     .about.rim.d.credits.f.f.6 -foreground blue -text Ugly\n\
for {set i 1} {$i<=6} {incr i} {\n\
    pack  .about.rim.d.credits.f.f.$i -side top -fill x -expand 0 -anchor n\n\
}\n\
\n\
frame     .about.rim.d.feedback\n\
frame     .about.rim.d.feedback.f -relief flat\n\
frame     .about.rim.d.feedback.f.f\n\
pack      .about.rim.d.feedback.f -fill both -expand 1\n\
pack      .about.rim.d.feedback.f.f -side left -fill x -expand 1\n\
label     .about.rim.d.feedback.f.f.1                  -text \"A mailing list exists for general comments and discussion:\"\n\
label     .about.rim.d.feedback.f.f.2 -foreground blue -text \"rat-users@cs.ucl.ac.uk\"\n\
label     .about.rim.d.feedback.f.f.3                  -text \"This list is open to all, you are encouraged to subscribe\"\n\
label     .about.rim.d.feedback.f.f.4                  -text \"(send mail to rat-users-request@cs.ucl.ac.uk to join).\\n\"\n\
label     .about.rim.d.feedback.f.f.5                  -text \"To reach the tool developers, send email to:\"\n\
label     .about.rim.d.feedback.f.f.6 -foreground blue -text \"rat-trap@cs.ucl.ac.uk\\n\"\n\
label     .about.rim.d.feedback.f.f.7                  -text \"Further information is available on the world-wide web at:\"\n\
label     .about.rim.d.feedback.f.f.8 -foreground blue -text \"http://www-mice.cs.ucl.ac.uk/multimedia/software/rat/\\n\"\n\
for {set i 1} {$i<=8} {incr i} {\n\
    pack  .about.rim.d.feedback.f.f.$i -side top -fill x\n\
}\n\
\n\
wm withdraw  .about\n\
wm title     .about \"About RAT\"\n\
wm resizable .about 0 0\n\
wm protocol  .about WM_DELETE_WINDOW {wm withdraw .about}\n\
\n\
set about_pane Copyright\n\
set_pane about_pane .about.rim.d \"Credits\"\n\
constrain_window .about $infofont 64 25\n\
\n\
.about.rim.d.copyright.f.f.blurb insert end \\\n\
{Copyright (C) 1995-2006 University College London\n\
All rights reserved.\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, is permitted provided that the following conditions\n\
are met:\n\
\n\
1. Redistributions of source code must retain the above copyright\n\
   notice, this list of conditions and the following disclaimer.\n\
\n\
2. Redistributions in binary form must reproduce the above copyright\n\
   notice, this list of conditions and the following disclaimer in the\n\
   documentation and/or other materials provided with the distribution.\n\
\n\
3. All advertising materials mentioning features or use of this software\n\
   must display the following acknowledgement:\n\
\n\
     This product includes software developed by the Computer Science\n\
     Department at University College London.\n\
\n\
4. Neither the name of the University nor of the Department may be used\n\
   to endorse or promote products derived from this software without\n\
   specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS\n\
``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING,\n\
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY\n\
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO\n\
EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY\n\
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n\
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,\n\
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n\
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED\n\
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n\
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n\
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF\n\
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
\n\
This software is derived, in part, from publically available\n\
and contributed source code with the following copyright:\n\
\n\
Copyright (C) 1991-1993,1996 Regents of the University of California\n\
Copyright (C) 1992 Stichting Mathematisch Centrum, Amsterdam\n\
Copyright (C) 1991-1992 RSA Data Security, Inc\n\
Copyright (C) 1992 Jutta Degener and Carsten Bormann, TU Berlin\n\
Copyright (C) 1994 Paul Stewart\n\
Copyright (C) 2000 Nortel Networks\n\
Copyright (C) 2000 Argonne National Laboratory\n\
Copyright (C) 1991 Bell Communications Research, Inc. (Bellcore)\n\
Copyright (C) 1996 Internet Software Consortium.\n\
Copyright (C) 1995-1999 WIDE Project\n\
\n\
This product includes software developed by the Computer\n\
Systems Engineering Group and by the Network Research Group\n\
at Lawrence Berkeley Laboratory.\n\
\n\
The WB-ADPCM algorithm was developed by British Telecommunications\n\
plc.  Permission has been granted to use it for non-commercial\n\
research and development projects.  BT retain the intellectual\n\
property rights to this algorithm.\n\
\n\
Encryption features of this software use the RSA Data\n\
Security, Inc. MD5 Message-Digest Algorithm.\n\
\n\
}\n\
\n\
proc shuffle_rats {args} {\n\
    # This should really animate the movement and play fruit-machine sounds.... :-)\n\
    set r \"\"\n\
    set end [llength $args]\n\
    set l 1\n\
    while { $l <= $end } {\n\
		set toget [expr abs([clock clicks]) % [llength $args]]\n\
		set r [format \"%s%s  \" $r [lindex $args $toget]]\n\
		set args [lreplace $args $toget $toget]\n\
		if {$l != $end && [expr $l % 3] == 0} {\n\
			set r \"$r\\n\"\n\
		}\n\
		incr l\n\
    }\n\
    return $r\n\
}\n\
\n\
proc jiggle_credits {} {\n\
# Software really developed by the Socialist Department of Computer Science\n\
    .about.rim.d.credits.f.f.2 configure -text [shuffle_rats \"Angela Sasse\" \"Vicky Hardman\" \"Peter Kirstein\"]\n\
    .about.rim.d.credits.f.f.4 configure -text [shuffle_rats \"Colin Perkins\" \"Orion Hodson\"]\n\
    .about.rim.d.credits.f.f.6 configure -text [shuffle_rats \"Isidor Kouvelas\" \"Darren Harris\" \"Anna Watson\" \"Mark Handley\" \"Jon Crowcroft\" \"Marcus Iken\" \"Kris Hasler\" \"Tristan Henderson\" \"Dimitrios Miras\" \"Socrates Varakliotis\" \"Piers O'Hanlon\"]\n\
}\n\
\n\
proc sync_ui_to_engine {} {\n\
    # the next time the display is shown, it needs to reflect the\n\
    # state of the audio engine.\n\
    mbus_send \"R\" \"tool.rat.settings\" \"\"\n\
}\n\
\n\
\n\
proc sync_engine_to_ui {} {\n\
    # make audio engine concur with ui - called when \"Apply\" button clicked\n\
    global my_ssrc rtcp_name rtcp_email rtcp_phone rtcp_loc rtcp_note\n\
    global prenc upp channel_var secenc layerenc red_off int_gap int_units\n\
    global agc_var audio_loop_var echo_var\n\
    global repair_var limit_var min_var max_var lecture_var 3d_audio_var convert_var\n\
    global meter_var gain volume iport oport\n\
    global in_mute_var out_mute_var ichannels freq key key_var\n\
    global audio_device received\n\
\n\
    #rtcp details\n\
    mbus_send \"R\" \"rtp.source.name\"  \"[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_name]\"\n\
    mbus_send \"R\" \"rtp.source.email\" \"[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_email]\"\n\
    mbus_send \"R\" \"rtp.source.phone\" \"[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_phone]\"\n\
    mbus_send \"R\" \"rtp.source.loc\"   \"[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_loc]\"\n\
    mbus_send \"R\" \"rtp.source.note\"  \"[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_note]\"\n\
\n\
    #cheat to update my details right away (it's disgusting)\n\
    mbus_recv_rtp.source.name  $my_ssrc \"$rtcp_name\"\n\
    mbus_recv_rtp.source.email $my_ssrc \"$rtcp_email\"\n\
    mbus_recv_rtp.source.phone $my_ssrc \"$rtcp_phone\"\n\
    mbus_recv_rtp.source.loc   $my_ssrc \"$rtcp_loc\"\n\
    mbus_recv_rtp.source.note  $my_ssrc \"$rtcp_note\"\n\
\n\
    #transmission details\n\
    mbus_send \"R\" \"tool.rat.codec\"      \"[mbus_encode_str $prenc] [mbus_encode_str $ichannels] [mbus_encode_str $freq]\"\n\
    mbus_send \"R\" \"tool.rat.rate\"         $upp\n\
\n\
    switch $channel_var {\n\
    	none         {mbus_send \"R\" \"audio.channel.coding\" \"[mbus_encode_str $channel_var]\"}\n\
	redundancy   {mbus_send \"R\" \"audio.channel.coding\" \"[mbus_encode_str $channel_var] [mbus_encode_str $secenc] $red_off\"}\n\
	interleaved {mbus_send \"R\" \"audio.channel.coding\" \"[mbus_encode_str $channel_var] $int_gap $int_units\"}\n\
	layering	{mbus_send \"R\" \"audio.channel.coding\" \"[mbus_encode_str $channel_var] [mbus_encode_str $prenc] [mbus_encode_str $ichannels] [mbus_encode_str $freq] $layerenc\"}\n\
    	*           {error \"unknown channel coding scheme $channel_var\"}\n\
    }\n\
\n\
    mbus_send \"R\" \"tool.rat.agc\"               $agc_var\n\
    mbus_send \"R\" \"tool.rat.loopback.gain\"     $audio_loop_var\n\
    mbus_send \"R\" \"tool.rat.echo.suppress\"     $echo_var\n\
\n\
    #Reception Options\n\
    mbus_send \"R\" \"audio.channel.repair\"   [mbus_encode_str $repair_var]\n\
    mbus_send \"R\" \"tool.rat.playout.limit\" $limit_var\n\
    mbus_send \"R\" \"tool.rat.playout.min\"   $min_var\n\
    mbus_send \"R\" \"tool.rat.playout.max\"   $max_var\n\
    mbus_send \"R\" \"tool.rat.lecture.mode\"  $lecture_var\n\
    mbus_send \"R\" \"audio.3d.enabled\"    $3d_audio_var\n\
    mbus_send \"R\" \"tool.rat.converter\"     [mbus_encode_str $convert_var]\n\
\n\
    #Security\n\
    if {$key_var==1 && [string length $key]!=0} {\n\
	mbus_send \"R\" \"security.encryption.key\" [mbus_encode_str $key]\n\
	update_security_indicator \n\
    } else {\n\
	mbus_send \"R\" \"security.encryption.key\" [mbus_encode_str \"\"]\n\
	update_security_indicator \n\
    }\n\
\n\
    #Interface\n\
    mbus_send \"R\" \"tool.rat.powermeter\"   $meter_var\n\
\n\
    #device\n\
    mbus_send \"R\" \"audio.device\"        [mbus_encode_str \"$audio_device\"]\n\
    if {$received(gain)} {\n\
	mbus_send \"R\" \"audio.input.gain\"    $gain\n\
    }\n\
    if {$received(volume)} {\n\
	mbus_send \"R\" \"audio.output.gain\"   $volume\n\
    }\n\
    mbus_send \"R\" \"audio.input.port\"    [mbus_encode_str $iport]\n\
    mbus_send \"R\" \"audio.output.port\"   [mbus_encode_str $oport]\n\
    mbus_send \"R\" \"audio.input.mute\"    $in_mute_var\n\
    mbus_send \"R\" \"audio.output.mute\"   $out_mute_var\n\
\n\
    send_silence_params\n\
}\n\
\n\
proc send_silence_params {args} {\n\
# These get special treatment and are sent as soon as they are changed\n\
# because if user is setting a threshold they need to tell how it works\n\
# immediately\n\
    global silence_var manual_silence_thresh\n\
    mbus_send \"R\" \"tool.rat.silence\"           [mbus_encode_str $silence_var]\n\
    mbus_send \"R\" \"tool.rat.silence.threshold\" $manual_silence_thresh\n\
}\n\
\n\
#\n\
# Routines to display the \"chart\" of RTCP RR statistics...\n\
#\n\
\n\
proc mtrace_callback {fd src dst} {\n\
	if [winfo exists .mtrace-$src-$dst] {\n\
		.mtrace-$src-$dst.t insert end [read $fd]\n\
	} else {\n\
		# The user has closed the mtrace window before the trace has completed\n\
		close $fd\n\
	}\n\
	if [eof $fd] {\n\
		close $fd\n\
	}\n\
}\n\
\n\
proc mtrace {src dst} {\n\
	global CNAME group_addr\n\
	regsub {.*@([0-9]+.[0-9]+.[0-9]+.[0-9]+)} $CNAME($src) {\\1} src_addr\n\
	regsub {.*@([0-9]+.[0-9]+.[0-9]+.[0-9]+)} $CNAME($dst) {\\1} dst_addr\n\
\n\
	toplevel  .mtrace-$src-$dst\n\
	text      .mtrace-$src-$dst.t -background white -font \"Courier 8\" -wrap none \\\n\
	                              -yscrollcommand \".mtrace-$src-$dst.sr set\" \\\n\
	                              -xscrollcommand \".mtrace-$src-$dst.sb set\"\n\
	if {$::tk_version < 8.5} {\n\
		scrollbar .mtrace-$src-$dst.sb -command \".mtrace-$src-$dst.t xview\" -orient horizontal\n\
		scrollbar .mtrace-$src-$dst.sr -command \".mtrace-$src-$dst.t yview\" -orient vertical\n\
	} else {\n\
		ttk::scrollbar .mtrace-$src-$dst.sb -command \".mtrace-$src-$dst.t xview\" -orient horizontal\n\
		ttk::scrollbar .mtrace-$src-$dst.sr -command \".mtrace-$src-$dst.t yview\" -orient vertical\n\
	}\n\
	pack .mtrace-$src-$dst.sb -fill x    -expand 0 -side bottom -anchor s\n\
	pack .mtrace-$src-$dst.sr -fill y    -expand 0 -side right  -anchor e\n\
	pack .mtrace-$src-$dst.t  -fill both -expand 1 -side left   -anchor w\n\
\n\
	wm title .mtrace-$src-$dst \"mtrace from $src_addr to $dst_addr via group $group_addr\"\n\
\n\
	if [file executable /usr/sbin/mtrace] {\n\
		set mtrace_prog /usr/sbin/mtrace\n\
	} else {\n\
		set mtrace_prog mtrace\n\
	}\n\
	set fd [open \"|$mtrace_prog $src_addr $dst_addr $group_addr\" \"r\"]\n\
	fconfigure $fd -blocking 0\n\
	fileevent  $fd readable \"mtrace_callback $fd $src $dst\"\n\
}\n\
\n\
chart::create\n\
\n\
#\n\
# End of RTCP RR chart routines\n\
#\n\
\n\
#\n\
# File Control Window\n\
#\n\
\n\
set play_file(state) end\n\
set rec_file(state) end\n\
\n\
catch {\n\
    toplevel .file\n\
    frame .file.play -relief groove -bd 2\n\
    frame .file.rec  -relief groove -bd 2\n\
    pack  .file.play -side top -pady 2 -padx 2 -fill x -expand 1\n\
    pack  .file.rec  -side top -pady 2 -padx 2 -fill x -expand 1\n\
\n\
    label .file.play.l -text \"Playback\"\n\
    pack  .file.play.l -side top -fill x\n\
    label .file.rec.l -text \"Record\"\n\
    pack  .file.rec.l -side top -fill x\n\
\n\
    wm withdraw .file\n\
    wm title	.file \"RAT File Control\"\n\
    wm protocol .file WM_DELETE_WINDOW \"set files_on 0; file_show\"\n\
\n\
    foreach action { play rec } {\n\
	frame  .file.$action.buttons\n\
	pack   .file.$action.buttons\n\
	button .file.$action.buttons.disk -bitmap disk -command \"fileDialog $action\"\n\
	pack   .file.$action.buttons.disk -side left -padx 2 -pady 2 -anchor n\n\
\n\
	foreach cmd \"$action pause stop\" {\n\
	    button .file.$action.buttons.$cmd -bitmap $cmd -state disabled -command file_$action\\_$cmd\n\
	    pack   .file.$action.buttons.$cmd -side left -padx 2 -pady 2 -anchor n -fill x\n\
	}\n\
\n\
	label  .file.$action.buttons.status -text \"No file selected.\" -width 16 -anchor w\n\
	pack   .file.$action.buttons.status -side bottom -fill both -expand 1 -padx 2 -pady 2\n\
    }\n\
} fwinerr\n\
\n\
if {$fwinerr != {}} {\n\
	bgerror \"$fwinerr\"\n\
}\n\
\n\
proc fileDialog {cmdbox} {\n\
    global win32 tcl_platform\n\
\n\
	set defaultExtension wav\n\
	set defaultLocation  [file nativename \"~/\"]\n\
\n\
    set types {\n\
		{\"NeXT/Sun Audio files\"	\"au\"}\n\
		{\"Microsoft RIFF files\"	\"wav\"}\n\
		{\"All files\"		\"*\"}\n\
    }\n\
\n\
    if {![string compare $cmdbox \"play\"]} {\n\
		catch { asFileBox .playfilebox  -defaultextension $defaultExtension -command file_open_$cmdbox -directory $defaultLocation -extensions $types } asferror\n\
    } else {\n\
		catch { asFileBox .recfilebox   -defaultextension $defaultExtension -command file_open_$cmdbox  -extensions $types -force_extension 1 } asferror\n\
    }\n\
\n\
	if {$asferror != \"\"} {\n\
		bgerror \"$asferror\"\n\
	}\n\
}\n\
\n\
proc file_show {} {\n\
    global files_on\n\
\n\
    if {$files_on} {\n\
	 	wm deiconify .file\n\
    } else {\n\
 		wm withdraw .file\n\
    }\n\
}\n\
\n\
proc file_play_live {} {\n\
# Request heart beat to determine if file is valid\n\
	mbus_send \"R\" audio.file.play.live \"\"\n\
}\n\
\n\
proc file_rec_live {} {\n\
# Request heart beat to determine if file is valid\n\
	mbus_send \"R\" audio.file.record.live \"\"\n\
}\n\
\n\
proc file_open_play {path} {\n\
    global play_file\n\
\n\
    mbus_send \"R\" \"audio.file.play.open\" [mbus_encode_str $path]\n\
    mbus_send \"R\" \"audio.file.play.pause\" 1\n\
    set play_file(state) paused\n\
    set play_file(name) $path\n\
\n\
    # Test whether file is still playing/valid\n\
    after 200 file_play_live\n\
}\n\
\n\
proc file_open_rec {path} {\n\
    global rec_file\n\
\n\
    mbus_send \"R\" \"audio.file.record.open\" [mbus_encode_str $path]\n\
    mbus_send \"R\" \"audio.file.record.pause\" 1\n\
\n\
    set rec_file(state) paused\n\
    set rec_file(name)  $path\n\
\n\
    # Test whether file is still recording/valid\n\
    after 200 file_rec_live\n\
}\n\
\n\
proc file_enable_play { } {\n\
    .file.play.buttons.play   configure -state normal\n\
    .file.play.buttons.pause  configure -state disabled\n\
    .file.play.buttons.stop   configure -state disabled\n\
    .file.play.buttons.status configure -text \"Ready to play.\"\n\
}\n\
\n\
proc file_enable_record { } {\n\
    .file.rec.buttons.rec configure -state normal\n\
    .file.rec.buttons.pause  configure -state disabled\n\
    .file.rec.buttons.stop   configure -state disabled\n\
    .file.rec.buttons.status configure -text \"Ready to record.\"\n\
}\n\
\n\
proc file_play_play {} {\n\
	global play_file\n\
\n\
	catch {\n\
		if {$play_file(state) == \"paused\"} {\n\
			mbus_send \"R\" \"audio.file.play.pause\" 0\n\
		} else {\n\
			mbus_send \"R\" \"audio.file.play.open\" [mbus_encode_str $play_file(name)]\n\
		}\n\
		set play_file(state) play\n\
	} pferr\n\
\n\
	if { $pferr != \"play\" } { bgerror \"pferr: $pferr\" }\n\
\n\
	.file.play.buttons.play   configure -state disabled\n\
	.file.play.buttons.pause  configure -state normal\n\
	.file.play.buttons.stop   configure -state normal\n\
	.file.play.buttons.status configure -text \"Playing.\"\n\
	after 200 file_play_live\n\
}\n\
\n\
proc file_play_pause {} {\n\
    global play_file\n\
\n\
    .file.play.buttons.play   configure -state normal\n\
    .file.play.buttons.pause  configure -state disabled\n\
    .file.play.buttons.stop   configure -state normal\n\
    .file.play.buttons.status configure -text \"Paused.\"\n\
    set play_file(state) paused\n\
    mbus_send \"R\" \"audio.file.play.pause\" 1\n\
}\n\
\n\
proc file_play_stop {} {\n\
	global play_file\n\
\n\
	set play_file(state) end\n\
	file_enable_play\n\
	mbus_send \"R\" \"audio.file.play.stop\" \"\"\n\
}\n\
\n\
proc file_rec_rec {} {\n\
	global rec_file\n\
\n\
	catch {\n\
		if {$rec_file(state) == \"paused\"} {\n\
			mbus_send \"R\" \"audio.file.record.pause\" 0\n\
		} else {\n\
			mbus_send \"R\" \"audio.file.record.open\" [mbus_encode_str $rec_file(name)]\n\
		}\n\
		set rec_file(state) record\n\
	} prerr\n\
\n\
	if { $prerr != \"record\" } { bgerror \"prerr: $prerr\" }\n\
\n\
	.file.rec.buttons.rec    configure -state disabled\n\
	.file.rec.buttons.pause  configure -state normal\n\
	.file.rec.buttons.stop   configure -state normal\n\
	.file.rec.buttons.status configure -text \"Recording.\"\n\
	after 200 file_rec_live\n\
}\n\
\n\
proc file_rec_pause {} {\n\
    global rec_file\n\
\n\
    .file.rec.buttons.rec    configure -state normal\n\
    .file.rec.buttons.pause  configure -state disabled\n\
    .file.rec.buttons.stop   configure -state normal\n\
    .file.rec.buttons.status configure -text \"Paused.\"\n\
    set rec_file(state) paused\n\
    mbus_send \"R\" \"audio.file.record.pause\" 1\n\
}\n\
\n\
proc file_rec_stop {} {\n\
	global rec_file\n\
\n\
	set rec_file(state) end\n\
	file_enable_record\n\
	mbus_send \"R\" \"audio.file.record.stop\" \"\"\n\
}\n\
\n\
#\n\
# End of File Window routines\n\
#\n\
\n\
help::add .r.c.tx.au.pow.sli 	\"This slider controls the volume\\nof the sound you send.\" \"tx_gain.au\"\n\
help::add .r.c.tx.au.port.l0 	\"Click to change input device.\" \"tx_port.au\"\n\
help::add .r.c.tx.au.port.l1  	\"Shows currently selected input device.\\nClick the arrows on either side to change.\" \"rx_port_name.au\"\n\
help::add .r.c.tx.au.port.l2 	\"Click to change input device.\" \"tx_port.au\"\n\
help::add .r.c.tx.net.on 	\"If this button is pushed in, you are are transmitting, and\\nmay be\\\n\
                                 heard by other participants. Holding down the\\nright mouse button in\\\n\
                                 any RAT window will temporarily\\ntoggle the state of this button,\\\n\
                                 allowing for easy\\npush-to-talk operation.\" \"tx_mute.au\"\n\
help::add .r.c.tx.au.pow.bar 	\"Indicates the loudness of the\\nsound you are sending. If this\\nis\\\n\
                                 moving, you may be heard by\\nthe other participants.\" \"tx_powermeter.au\"\n\
\n\
help::add .r.c.rx.au.pow.sli  	\"This slider controls the volume\\nof the sound you hear.\" \"rx_gain.au\"\n\
help::add .r.c.rx.au.port.l0  	\"Click to change output device.\" \"rx_port.au\"\n\
help::add .r.c.rx.au.port.l1  	\"Shows currently selected output device.\\nClick the arrows on either side to change.\" \"rx_port_name.au\"\n\
help::add .r.c.rx.au.port.l2  	\"Click to change output device.\" \"rx_port.au\"\n\
help::add .r.c.rx.net.on  	\"If pushed in, reception is muted.\" \"rx_mute.au\"\n\
help::add .r.c.rx.au.pow.bar  	\"Indicates the loudness of the\\nsound you are hearing.\" \"rx_powermeter.au\"\n\
\n\
help::add .r.f		\"Name of the session, and the IP address, port\\n&\\\n\
		 	 TTL used to transmit the audio data.\" \"session_title.au\"\n\
help::add .l.t		\"The participants in this session with you at the top.\\nClick on a name\\\n\
                         with the left mouse button to display\\ninformation on that participant,\\\n\
			 and with the middle\\nbutton to mute that participant (the right button\\nwill\\\n\
			 toggle the transmission mute button, as usual).\" \"participant_list.au\"\n\
\n\
help::add .st.recp 	\"Displays a chart showing the reception\\nquality reported by all participants\" \"rqm_enable.au\"\n\
help::add .st.help       \"If you can see this, balloon help\\nis enabled. If not, it isn't.\" \"balloon_help.au\"\n\
help::add .st.opts   	\"Brings up another window allowing\\nthe control of various options.\" \"options.au\"\n\
help::add .st.about  	\"Brings up another window displaying\\ncopyright & author information.\" \"about.au\"\n\
help::add .st.quit   	\"Press to leave the session.\" \"quit.au\"\n\
\n\
# preferences help\n\
help::add .prefs.m.f.m  \"Click here to change the preference\\ncategory.\" \"prefs_category.au\"\n\
set i .prefs.buttons\n\
help::add $i.bye         \"Cancel changes.\" \"cancel_changes.au\"\n\
help::add $i.apply       \"Apply changes.\" \"apply_changes.au\"\n\
\n\
# user help\n\
set i .prefs.pane.personal.a.f.f.ents\n\
help::add $i.name      	\"Enter your name for transmission\\nto other participants.\" \"user_name.au\"\n\
help::add $i.email      	\"Enter your email address for transmission\\nto other participants.\" \"user_email.au\"\n\
help::add $i.phone     	\"Enter your phone number for transmission\\nto other participants.\" \"user_phone.au\"\n\
help::add $i.loc      	\"Enter your location for transmission\\nto other participants.\" \"user_loc.au\"\n\
\n\
#audio help\n\
set i .prefs.pane.audio\n\
help::add $i.dd.device.mdev \"Selects preferred audio device.\" \"audio_device.au\"\n\
help::add $i.dd.sampling.freq.mb \\\n\
                        \"Sets the sampling rate of the audio device.\\nThis changes the available codecs.\" \"sampling_freq.au\"\n\
help::add $i.dd.sampling.ch_in.mb \\\n\
                        \"Changes between mono and stereo audio input.\" \"mono_stereo.au\"\n\
\n\
help::add $i.dd.cks.f.f.silence.upper.r0\\\n\
			 \"All audio is transmitted when the input is unmuted.\" \"suppress_silence_off.au\"\n\
help::add $i.dd.cks.f.f.silence.upper.r1\\\n\
			 \"Only audio above an automatically determined threshold\\nis transmitted when the input is unmuted.\" \"suppress_silence_auto.au\"\n\
help::add $i.dd.cks.f.f.silence.upper.r2\\\n\
			 \"Only audio above a manually set threshold is\\ntransmitted when the input is unmuted.\" \"suppress_silence_manual.au\"\n\
help::add $i.dd.cks.f.f.silence.lower.s\\\n\
			 \"Sets the manual threshold silence detection threshold.\" \"suppress_silence_manual_thresh.au\"\n\
\n\
help::add $i.dd.cks.f.f.other.agc	 \"Enables automatic control of the volume\\nof the sound you send.\" \"agc.au\"\n\
help::add $i.dd.cks.f.f.other.loop \"Enables hardware for loopback of audio input.\" \"audio_loopback.au\"\n\
help::add $i.dd.cks.f.f.other.suppress \\\n\
                         \"Mutes microphone when playing audio.\" \"echo_suppression.au\"\n\
help::add $i.dd.cks.f.f.other.tone \\\n\
                         \"Plays a local test tone (not transmitted).\" \"test_tone.au\"\n\
\n\
# transmission help\n\
set i .prefs.pane.transmission\n\
\n\
help::add $i.dd.units.m	\"Sets the duration of each packet sent.\\nThere is a fixed per-packet\\\n\
                         overhead, so\\nmaking this larger will reduce the total\\noverhead.\\\n\
                         The effects of packet loss are\\nmore noticable with large packets.\" \"packet_duration.au\"\n\
help::add $i.dd.pri.m	\"Changes the primary audio compression\\nscheme. The list is arranged\\\n\
                         with high-\\nquality, high-bandwidth choices at the\\ntop, and\\\n\
                         poor-quality, lower-bandwidth\\nchoices at the bottom.\" \"codec_primary.au\"\n\
help::add $i.cc.van.rb	\"Sets no channel coding.\" \"channel_coding_none.au\"\n\
help::add $i.cc.red.rb	\"Piggybacks earlier units of audio into packets\\n\\\n\
			 to protect against packet loss. Some audio\\n\\\n\
			 tools (eg: vat-4.0) are not able to receive\\n\\\n\
			 audio sent with this option.\" \"channel_coding_redundant.au\"\n\
help::add $i.cc.red.fc.m \\\n\
			\"Sets the format of the piggybacked data.\" \"red_codec.au\"\n\
help::add $i.cc.red.u.m \\\n\
			\"Sets the offset of the piggybacked data.\" \"red_offset.au\"\n\
help::add $i.cc.layer.fc.m  \"Sets the number of discrete layers which will\\nbe sent. You need\\\n\
                            to start RAT with the options\\n-l n <address>/<port> <address>/<port>,\\nwhere\\\n\
                            n is the number of layers and there is an\\naddress and port for each layer.\\\n\
                            NB: this is only\\nsupported by the WBS codec at present.\" \"num_layers.au\"\n\
help::add $i.cc.int.fc.m \\\n\
			\"Sets the separation of adjacent units within\\neach packet. Larger values correspond\\\n\
			 to longer\\ndelays.\" \"int_sep.au\"\n\
help::add $i.cc.int.zz.m \"Number of compound units per packet.\" \"int_units.au\"\n\
help::add $i.cc.int.rb	\"Enables interleaving which exchanges latency\\n\\\n\
			 for protection against burst losses.  No other\\n\\\n\
			 audio tools can decode this format (experimental).\" \"channel_coding_interleave.au\"\n\
\n\
# Reception Help\n\
set i .prefs.pane.reception\n\
help::add $i.r.m 	\"Sets the type of repair applied when packets are\\nlost. The schemes\\\n\
			 are listed in order of increasing\\ncomplexity and quality of repair.\" \"repair_schemes.au\"\n\
help::add $i.r.ms        \"Sets the type of sample rate conversion algorithm\\n\\\n\
			 that will be applied to streams that differ in rate\\n\\\n\
			 to the audio device rate.\" \"samples_rate_conversion.au\"\n\
help::add $i.o.f.cb      \"Enforce playout delay limits set below.\\nThis is not usually desirable.\" \"playout_limits.au\"\n\
help::add $i.o.f.fl.scmin   \"Sets the minimum playout delay that will be\\napplied to incoming\\\n\
			 audio streams.\" \"min_playout_delay.au\"\n\
help::add $i.o.f.fr.scmax   \"Sets the maximum playout delay that will be\\napplied to incoming\\\n\
			 audio streams.\" \"max_playout_delay.au\"\n\
help::add $i.c.f.f.lec  	\"If enabled, extra delay is added at both sender and receiver.\\nThis allows\\\n\
                         the receiver to better cope with host scheduling\\nproblems, and the sender\\\n\
			 to perform better silence suppression.\\nAs the name suggests, this option\\\n\
			 is intended for scenarios such\\nas transmitting a lecture, where interactivity\\\n\
			 is less important\\nthan quality.\" \"lecture_mode.au\"\n\
\n\
# security...too obvious for balloon help!\n\
help::add .prefs.pane.security.a.f.f.e \"Due to government export restrictions\\nhelp\\\n\
				       for this option is not available.\" \"security.au\"\n\
\n\
# interface...ditto!\n\
set i .prefs.pane.interface\n\
help::add $i.a.f.f.power \"Disable display of audio powermeters. This\\nis only\\\n\
		 	 useful if you have a slow machine\" \"disable_powermeters.au\"\n\
help::add $i.a.f.f.balloon \"If you can see this, balloon help\\nis enabled. If not, it isn't.\" \"balloon_help.au\"\n\
help::add $i.a.f.f.matrix  \"Displays a chart showing the reception\\nquality reported by all participants\" \"rqm_enable.au\"\n\
help::add $i.a.f.f.plist   \"Hides the list of participants\" \"participant_list.au\"\n\
\n\
help::add .chart		\"This chart displays the reception quality reported\\n by all session\\\n\
                         participants. Looking along a row\\n gives the quality with which that\\\n\
			 participant was\\n received by all other participants in the session:\\n green\\\n\
			 is good quality, orange medium quality, and\\n red poor quality audio.\" \"rqm.au\"\n\
\n\
proc rendezvous_with_media_engine {} {\n\
	mbus_send \"R\" \"tool.rat.settings\" \"\"\n\
	mbus_send \"R\" \"audio.query\" \"\"\n\
	mbus_send \"R\" \"rtp.query\" \"\"\n\
}\n\
\n\
} script_evaluation_error ]\n\
\n\
if {$script_error != 0} {\n\
        bgerror \"$script_evaluation_error\"\n\
}\n\
\n\
catch {console hide}\n\
";
