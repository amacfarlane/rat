namespace eval chart {
	proc create {} {
		toplevel  .chart
		canvas    .chart.c  -xscrollcommand {.chart.sb set} -yscrollcommand {.chart.sr set}
		frame     .chart.c.f
		if {$::tk_version < 8.5} {
			scrollbar .chart.sr -orient vertical   -command {.chart.c yview}
			scrollbar .chart.sb -orient horizontal -command {.chart.c xview}
		} else {
			ttk::scrollbar .chart.sr -orient vertical   -command {.chart.c yview}
			ttk::scrollbar .chart.sb -orient horizontal -command {.chart.c xview}
		}
		pack .chart.sb -side bottom -fill x    -expand 0 -anchor s
		pack .chart.sr -side right  -fill y    -expand 0 -anchor e
		pack .chart.c  -side left   -fill both -expand 1 -anchor n

		.chart.c create window 0 0 -anchor nw -window .chart.c.f

		toplevel .chart_popup       -bg black
		label    .chart_popup.text  -bg lavender -justify left
		pack .chart_popup.text -side top -anchor w -fill x
		wm transient        .chart_popup .
		wm withdraw         .chart_popup
		wm overrideredirect .chart_popup true

		wm withdraw .chart
		wm title    .chart "Reception quality matrix"
		wm geometry .chart 320x200
		wm protocol .chart WM_DELETE_WINDOW chart::hide
	}

	# Private - do not use outside this namespace
	proc popup_show {window} {
		variable chart_popup_src
		variable chart_popup_dst
		variable chart_popup_id
		global   NAME
		.chart_popup.text  configure -text "From: $NAME($chart_popup_src($window))\nTo: $NAME($chart_popup_dst($window))"
		# Beware! Don't put the popup under the cursor! Else we get window enter
		# for .help and leave for $window, making us hide_help which removes the
		# help window, giving us a window enter for $window making us popup the
		# help again.....
		if {[winfo width $window] > [winfo height $window]} {
			set xpos [expr [winfo pointerx $window] + 10]
			set ypos [expr [winfo rooty    $window] + [winfo height $window] + 5]
		} else {
			set xpos [expr [winfo rootx    $window] + [winfo width $window] + 5]
			set ypos [expr [winfo pointery $window] + 10]
		}

		wm geometry  .chart_popup +$xpos+$ypos
		set chart_popup_id [after 100 wm deiconify .chart_popup]
		raise .chart_popup $window
	}

	# Private - do not use outside this namespace
	proc popup_hide {window} {
		variable chart_popup_id
		if {[info exists chart_popup_id]} {
			after cancel $chart_popup_id
		}
		wm withdraw .chart_popup
	}

	# Private - do not use outside this namespace
	proc popup_add {window src dst} {
		variable chart_popup_src
		variable chart_popup_dst
		set chart_popup_src($window) $src
		set chart_popup_dst($window) $dst
		bind $window <Enter>    "+chart::popup_show $window"
		bind $window <Leave>    "+chart::popup_hide $window"
	}

	proc add {ssrc} {
		global NAME
		frame  .chart.c.f.$ssrc
		frame  .chart.c.f.$ssrc.f
		button .chart.c.f.$ssrc.l -width 25 -text $ssrc -padx 0 -pady 0 -anchor w -relief flat -command "toggle_stats $ssrc"
		pack   .chart.c.f.$ssrc.f -expand 0 -anchor e -side right
		pack   .chart.c.f.$ssrc.l -expand 1 -anchor w -fill x -side left
		pack   .chart.c.f.$ssrc   -expand 1 -anchor n -fill x -side top
		foreach s [pack slaves .chart.c.f] {
			regsub {.chart.c.f.(.*)} $s {\1} s
			button .chart.c.f.$s.f.$ssrc -width 5 -text "" -background white -padx 0 -pady 0 -command "mtrace $s $ssrc"
			pack   .chart.c.f.$s.f.$ssrc -expand 0 -side left
			chart::popup_add .chart.c.f.$s.f.$ssrc $s $ssrc
			if {![winfo exists .chart.c.f.$ssrc.f.$s]} {
				button .chart.c.f.$ssrc.f.$s -width 5 -text "" -background white -padx 0 -pady 0 -command "mtrace $ssrc $s"
				pack   .chart.c.f.$ssrc.f.$s -expand 0 -side left
				chart::popup_add .chart.c.f.$ssrc.f.$s $ssrc $s
			}
		}
		update
		.chart.c configure -scrollregion "0.0 0.0 [winfo width .chart.c.f] [winfo height .chart.c.f]"
	}

	proc remove {ssrc} {
		destroy .chart.c.f.$ssrc
		foreach s [pack slaves .chart.c.f] {
			regsub {.chart.c.f.(.*)} $s {\1} s
			destroy .chart.c.f.$s.f.$ssrc
		}
		update
		.chart.c configure -scrollregion "0.0 0.0 [winfo width .chart.c.f] [winfo height .chart.c.f]"
	}

	proc setlabel {ssrc label} {
		.chart.c.f.$ssrc.l configure -text $label
	}

	proc setvalue {src dst val} {
		if {$val < 5} {
			set colour green
			set txtval "$val%"
		} elseif {$val < 10} {
			set colour orange
			set txtval "$val%"
		} elseif {$val <= 100} {
			set colour red
			set txtval "$val%"
		} else {
			set colour white
			set txtval { }
		}
		.chart.c.f.$src.f.$dst configure -highlightbackground $colour -bg $colour -text "$txtval"
	}

	proc show {} {
		wm deiconify .chart
	}

	proc hide {} {
		wm withdraw .chart
	}
}
