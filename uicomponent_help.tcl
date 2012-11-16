namespace eval help {
	global help_on
	set help_on 0

	proc show {window} {
		variable help_text
		global help_on
		variable help_id
		variable help_clip
		if {$help_on} {
			.help.text  configure -text $help_text($window)
			# Beware! Don't put the popup under the cursor! Else we get window enter
			# for .help and leave for $window, making us hide_help which removes the
			# help window, giving us a window enter for $window making us popup the
			# help again.....
			if {[winfo width $window] > [winfo height $window]} {
			    set xpos [expr [winfo pointerx $window] + 10]
			    set ypos [expr [winfo rooty    $window] + [winfo height $window] + 4]
			} else {
			    set xpos [expr [winfo rootx    $window] + [winfo width $window] + 4]
			    set ypos [expr [winfo pointery $window] + 10]
			}

			wm geometry  .help +$xpos+$ypos
			set help_id [after 100 wm deiconify .help]
			raise .help $window
			if {[info exists help_clip($window)]} {
			    mbus_send "R" "tool.rat.voxlet.play" "\"$help_clip($window)\""
			}
		}
	}

	proc hide {window} {
		variable help_id
		global help_on
		if {[info exists help_id]} {
			after cancel $help_id
		}
		wm withdraw .help
	}

	proc add {window text clip} {
		variable help_text
		variable help_clip
		set help_text($window)  $text
		set help_clip($window)  $clip
		bind $window <Enter>    "+help::show $window"
		bind $window <Leave>    "+help::hide $window"
	}

	bind Entry   <KeyPress> "+help::hide %W"
	toplevel .help       -bg black
	label    .help.text  -bg lavender -justify left
	pack .help.text -side top -anchor w -fill x
	wm transient        .help .
	wm withdraw         .help
	wm overrideredirect .help true
}

