

# Asynchronous file box -the default one is modal and no good for real time applications.
#
# Usage: asFileBox .widget
#
# Options: -command <command>
#          -defaultextension <extension>
#          -directory  <startdir>
#          -extensions <extension list>
#          -force_extension <bool>       - append extension to name if not entered
#          -type    <button label>       - Open/Save
#          -title <title>
# E.g.
# set extensions {{"NeXT/Sun Audio Files" "au"} {"Scriptics TCL Files" "tcl"} {"Microsoft RIFF files" "wav"} {"All Files" "*"}}
# asFileBox .zzz -command puts -extensions $extensions -defaultextension au

proc asParseArgs {dstArray commands options} {
    upvar $dstArray out
    upvar $options opts

    # Keep note of default values
    foreach i $commands {
	set field [lindex $i 0]
	set value [lindex $i 1]
	set out($field) $value
    }

    # Read all options -whatever value
    for {set i 0} {$i < [llength $opts]} {incr i} {
	if {[string index [lindex $opts $i] 0] == "-"} {
	    set out([lindex $opts $i]) [lindex $opts [incr i]]
	}
    }
}

# Returns .root from .root.sub1.sub2.etc
proc asRootName {path} {
    set out [split $path .]
    return .[lindex $out 1]
}

proc asUpdir {path} {
    set l [split [string trimright $path /] /]
    set end [expr [llength $l] - 1]

    if {$end <= 0} {
	return $path
    }

    for {set i 0} {$i < $end} {incr i} {
	append updir [lindex $l $i] /
    }
    return $updir
}

proc asSelectFile {w value} {
    global as_cwd

    $w.main.entries.e delete 0 end

    if [file isfile $as_cwd($w)/$value] {
	$w.main.entries.e insert 0 $value
    }
}

proc asOpenFile {w value} {
    global as_cwd as_ok as_file

    set value [string trimright $value /]
    set value [string trim $value \[\]]
    if {$value == ".."} {
		catch {
			set as_cwd($w) [asUpdir $as_cwd($w)]
			cd $as_cwd($w)
		}
		asFilter $w
    } elseif [string match "?:" $value] {
		catch {
			cd $value/
			set as_cwd($w) [pwd]
		}
		asFilter $w
    } elseif [file isdirectory $as_cwd($w)/$value] {
		set as_file($w) ""
		catch {
		    cd $as_cwd($w)/$value
		    set as_cwd($w) [pwd]
		}
		asFilter $w
    } elseif [file isfile $as_cwd($w)/$value] {
		tkButtonInvoke $as_ok($w)
    }
}

proc asFixName {w} {
    global as_ext as_file

    if {[string compare [string tolower [file extension $as_file($w)] ] .$as_ext($w)] \
	    && $as_ext($w) != "*" } {
	set as_file($w) "$as_file($w).$as_ext($w)"
    }
}

proc asCommand {w} {
    global as_cwd as_ext as_list as_file as_cmd as_ok as_fix_name

# Only perform actions if file exists
    if {$as_file($w) != ""} {
	if $as_fix_name($w) {
	    asFixName $w
	}
	$as_cmd($w) $as_cwd($w)/$as_file($w)
    }

    unset as_cwd($w)
    unset as_ext($w)
    unset as_list($w)
    unset as_file($w)
    unset as_cmd($w)
    unset as_ok($w)
    unset as_fix_name($w)

    destroy $w
}

proc asFilter {w} {
    global as_cwd as_ext as_list tcl_platform

    $as_list($w) delete 0 end

    cd $as_cwd($w)

    # Check if directory above exists
    set updir [asUpdir $as_cwd($w)]

    if {$updir != $as_cwd($w)} {
	set allfiles ".. [lsort [glob -nocomplain *]]"
    } else {
	set allfiles "[lsort [glob -nocomplain *]]"
	if {[string match "Windows*" "$tcl_platform(os)"]} {
	    set drives [file volume]
	}
    }

    set ext $as_ext($w)

    # Split into files and directories
    foreach f $allfiles {
	#This stops windows getting excited about paths beginning ~
	if {[file exists $f]} {
	    if {[file isdirectory $f]} {
		lappend dirs "$f/"
	    } elseif {[file isfile $f]} {
		if {$ext == "*"} {
		    lappend files $f
		} elseif {[string tolower [file extension $f]] == ".$ext"} {
		    lappend files $f
		}
	    }
	}
    }
    if [info exists drives] {
	foreach f $drives {
	    $as_list($w) insert end "\[[string trimright $f /]\]"
	}
    }

    if [info exists dirs] {
	foreach f $dirs {
	    $as_list($w) insert end "[file tail $f]/"
	}
    }
    if [info exists files] {
	foreach f $files {
	    $as_list($w) insert end $f
	}
    }
}

proc asFileBox {w args} {
    upvar $w base
    global as_cwd as_ext as_list as_file as_cmd as_ok as_fix_name

    # Process options
    set commands {
	{"-command"          "puts"}
	{"-defaultextension" ""}
	{"-directory"        "."}
	{"-extensions"       ""}
	{"-force_extension"   0}
	{"-title"            "Open File"}
	{"-type"             "Open"}
    }

    asParseArgs base $commands args

    # Save command
    set as_cmd($w) $base(-command)
    set as_file($w) ""

    # Do we return name with extension appended ?
    set as_fix_name($w) $base(-force_extension)

    # Create Window
    toplevel $w
    wm title $w $base(-title)
    wm geometry $w 400x320
    wm resizable $w 0 0

    frame $w.main
    pack  $w.main -fill both -expand 1 -padx 4 -pady 4

    # Create Path display
    frame $w.main.path
    label $w.main.path.l -text "Path: "
    label $w.main.path.lp  -justify left -textvariable as_cwd($w)
    pack  $w.main.path -fill x -expand 0
    pack  $w.main.path.l  -side left
    pack  $w.main.path.lp -side left -anchor e

    # Create file list
    frame $w.main.lbf
    if {$::tk_version < 8.5} {
	scrollbar $w.main.lbf.scroll -command "$w.main.lbf.list yview"
    } else {
	ttk::scrollbar $w.main.lbf.scroll -command "$w.main.lbf.list yview"
    }
    set as_list($w) [listbox $w.main.lbf.list -yscroll "$w.main.lbf.scroll set" -bg white]
    pack $w.main.lbf -side top -expand 1 -fill both
    pack $w.main.lbf.scroll -side right -fill y
    pack $w.main.lbf.list -side left -expand 1 -fill both

    # Create Frame for Labels
    frame $w.main.labels
    pack  $w.main.labels -side left
    label $w.main.labels.filename -text "File name:"
    label $w.main.labels.types -text "File type:"
    pack  $w.main.labels.filename $w.main.labels.types -side top -fill x -ipady 4

    # Add Entryframe and file types menubutton
    frame $w.main.entries
    entry $w.main.entries.e -textvariable as_file($w) -bg white
    menubutton $w.main.entries.mb -menu $w.main.entries.mb.menu  -indicatoron 1 -relief raised -width 20 -justify left
    menu $w.main.entries.mb.menu -tearoff 0
    pack $w.main.entries -side left -fill x -expand 1 -ipady 8 -ipadx 8 -padx 8
    pack $w.main.entries.e $w.main.entries.mb -side top -fill x -expand 1

    # Pack buttons
    frame $w.main.cmdf
    set as_ok($w) [button $w.main.cmdf.ok -text $base(-type) -command "asCommand $w"]
    button $w.main.cmdf.cancel -text "Cancel" -command "destroy $w"
    pack $w.main.cmdf -side right -fill both -expand 0
    pack $w.main.cmdf.ok     -side top    -expand 1 -fill x
    pack $w.main.cmdf.cancel -side bottom -expand 1 -fill x

    # Fill in extensions
    if {$base(-defaultextension) == ""} {
	set base(-defaultextension) [lindex [lindex $base(-extensions) 0] 1]
	puts "$base(-defaultextension)"
    }

    foreach i $base(-extensions) {
	set wording [lindex $i 0]
	set ext     [lindex $i 1]
	$w.main.entries.mb.menu add command -label "$wording (*.$ext)" -command "set as_ext($w) $ext; asFilter $w; $w.main.entries.mb configure -text \"$wording (*.$ext)\""
	if {$ext == $base(-defaultextension)} {
	    $w.main.entries.mb configure -text "$wording (*.$ext)"
	}
    }

    # Set up global storage for variables
    set as_cwd($w) [pwd]
    if {$base(-directory) != "."} {
	set as_cwd($w) $base(-directory)
    }

    set as_ext($w) $base(-defaultextension)

	# Add listbox bindings
    bind $w.main.lbf.list <1> {
		%W activate @%x,%y
		asSelectFile [asRootName %W] [%W get active]
    }

    bind $w.main.lbf.list <Double-Button-1> {
		%W activate @%x,%y
		asOpenFile [asRootName %W] [%W get active]
    }

	bind $w.main.lbf.list <KeyPress-Return> {

		asOpenFile [asRootName %W] [%W get active]
    }

	bind $w <Up> {
		set sel [%W.main.lbf.list curselection]
		if {$sel != {}} {
			incr sel -1
			if { $sel > -1 } {
				%W.main.lbf.list selection clear 0 end
				%W.main.lbf.list selection set $sel
				%W.main.lbf.list yview $sel
			asSelectFile [asRootName %W] [%W.main.lbf.list get active]
			}
		}
    }

	bind $w <Down> {
		set sel [%W.main.lbf.list curselection]
		if {$sel != {}} {
			incr sel
			if { $sel < [%W.main.lbf.list index end] } {
				%W.main.lbf.list selection clear 0 end
				%W.main.lbf.list selection set $sel
				%W.main.lbf.list yview $sel
				asSelectFile [asRootName %W] [%W.main.lbf.list get $sel]
			}
		}
    }

    asFilter $w
}




