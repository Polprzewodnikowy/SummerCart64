#!/usr/bin/expect

set timeout -1

set version [lindex $argv 0]

spawn ./quartus/setup.sh

expect {
    "Press \\\[Enter\\\] to continue:" { send "\r"; exp_continue }
    "Do you accept this license? \\\[y/n\\\]" { send "y\r"; exp_continue }
    "Installation directory \\\[/root/intelFPGA_lite/$version\\\]:" { send "/opt/intelFPGA_lite/$version\r"; exp_continue }
    "Quartus Prime Lite Edition (Free)  \\\[Y/n\\\] :" { send "y\r"; exp_continue }
    "Quartus Prime Lite Edition (Free)  - Quartus Prime Help" { send "n\r"; exp_continue }
    "Quartus Prime Lite Edition (Free)  - Devices \\\[Y/n\\\] " { send "y\r"; exp_continue }
    "Quartus Prime Lite Edition (Free)  - Devices - MAX 10 FPGA" { send "y\r"; exp_continue }
    "Quartus Prime Lite Edition (Free)  - Devices - " { send "n\r"; exp_continue }
    "ModelSim - Intel FPGA Starter Edition (Free)" { send "n\r"; exp_continue }
    "ModelSim - Intel FPGA Edition" { send "n\r"; exp_continue }
    "Is the selection above correct? \\\[Y/n\\\]:" { send "y\r"; exp_continue }
    "Create shortcuts on Desktop \\\[Y/n\\\]:" { send "n\r"; exp_continue }
    "Launch Quartus Prime Lite Edition \\\[Y/n\\\]:" { send "n\r"; exp_continue }
    "Provide your feedback at" { send "n\r"; exp_continue }
    eof { }
}
