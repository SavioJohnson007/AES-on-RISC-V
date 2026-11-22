# open_hw_manager
# connect_hw_server
# open_hw_target

# current_hw_device [lindex [get_hw_devices] 0]
# refresh_hw_device [current_hw_device]

# set_property PROGRAM.FILE $bitfile [current_hw_device]

# program_hw_devices [current_hw_device]

# close_hw_manager
# exit

#------------------------------------------------------------
# Vivado FPGA Programming Script
#------------------------------------------------------------

set script_dir [file dirname [file normalize [info script]]]
set bitfile [file join $script_dir ibex-demo-system build lowrisc_ibex_demo_system_0 synth_urbana-vivado lowrisc_ibex_demo_system_0.bit]

open_hw_manager
connect_hw_server

# -----------------------------------------------------------
# List available JTAG targets
# -----------------------------------------------------------
set targets [get_hw_targets]

if {[llength $targets] == 0} {
    puts "\nERROR: No JTAG targets found."
    close_hw_manager
    exit 1
}

puts "\nAvailable JTAG Targets:"
for {set i 0} {$i < [llength $targets]} {incr i} {
    puts "  $i : [lindex $targets $i]"
}

puts -nonewline "\nSelect target index: "
flush stdout
gets stdin target_idx

if {$target_idx < 0 || $target_idx >= [llength $targets]} {
    puts "Invalid target index."
    close_hw_manager
    exit 1
}

set target [lindex $targets $target_idx]

open_hw_target $target

# -----------------------------------------------------------
# List FPGA devices on selected target
# -----------------------------------------------------------
set devices [get_hw_devices]

if {[llength $devices] == 0} {
    puts "\nERROR: No FPGA devices found on selected target."
    close_hw_manager
    exit 1
}

puts "\nAvailable FPGA Devices:"
for {set i 0} {$i < [llength $devices]} {incr i} {
    puts "  $i : [lindex $devices $i]"
}

puts -nonewline "\nSelect device index: "
flush stdout
gets stdin device_idx

if {$device_idx < 0 || $device_idx >= [llength $devices]} {
    puts "Invalid device index."
    close_hw_manager
    exit 1
}

set device [lindex $devices $device_idx]

current_hw_device $device
refresh_hw_device $device

puts "\nProgramming $device ..."

set_property PROGRAM.FILE $bitfile $device
program_hw_devices $device

puts "\nProgramming completed successfully."

close_hw_manager
exit