set flow [lindex $quartus(args) 0]

if [string match "compile" $flow] {
    post_message "Generating final programming file"
    qexec "quartus_cpf -c SummerCart64.cof"
}
