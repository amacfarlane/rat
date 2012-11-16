namespace eval bargraph {
	variable litColors   [list #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #00cc00 #2ccf00 #58d200 #84d500 #b0d800 #dddd00 #ddb000 #dd8300 #dd5600 #dd2900]
	variable unlitColors [list #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #006600 #166700 #2c6900 #426a00 #586c00 #6e6e00 #6e5800 #6e4100 #6e2b00 #6e1400]
	variable totalHeight [llength $litColors]
	variable prevHeight

	proc create {bgraph} {
		variable oh$bgraph
		variable totalHeight
		variable unlitColors
		variable prevHeight

		frame $bgraph -bd 0 -bg black
        	for {set i 0} {$i < $totalHeight} {incr i} {
                	frame $bgraph.inner$i -bg "[lindex $unlitColors $i]" -width 4 -height 6
                	pack  $bgraph.inner$i -side left -fill both -expand true -padx 1 -pady 1
        	}
        	set prevHeight($bgraph) 0
	}

	proc setHeight {bgraph height} {
		variable prevHeight
		variable totalHeight
		variable litColors
		variable unlitColors

		if {$prevHeight($bgraph) > $height} {
			for {set i [expr $height]} {$i <= $prevHeight($bgraph)} {incr i} {
				$bgraph.inner$i config -bg "[lindex $unlitColors $i]"
			}
		} else {
		    for {set i [expr $prevHeight($bgraph)]} {$i <= $height} {incr i} {
			$bgraph.inner$i config -bg  "[lindex $litColors $i]"
		    }
		}
		set prevHeight($bgraph) $height
	}

	proc disable {bgraph} {
		variable totalHeight

		if {![winfo exists $bgraph]} {
			error "Bar graph $bgraph does not exist"
		}
		for {set i 0} {$i < $totalHeight} {incr i} {
			$bgraph.inner$i config -bg black
		}
	}

	proc enable {bgraph} {
		variable totalHeight
		variable unlitColors
		variable litColors
		variable prevHeight

		if {![winfo exists $bgraph]} {
			error "Bar graph $bgraph does not exist"
		}
		for {set i 0} {$i < $totalHeight} {incr i} {
			$bgraph.inner$i config -bg "[lindex $unlitColors $i]"
		}
		for {set i 0} {$i < $prevHeight($bgraph)} {incr i} {
			$bgraph.inner$i config -bg "[lindex $litColors $i]"
		}
	}
}

