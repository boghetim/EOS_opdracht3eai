# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\Users\boghe\EOS_opdracht3eai\vraag1\udp\vitis\design_1_wrapper\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\Users\boghe\EOS_opdracht3eai\vraag1\udp\vitis\design_1_wrapper\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {design_1_wrapper}\
-hw {C:\Users\boghe\EOS_opdracht3eai\vraag1\udp\design_1_wrapper.xsa}\
-out {C:/Users/boghe/EOS_opdracht3eai/vraag1/udp/vitis}

platform write
domain create -name {freertos10_xilinx_ps7_cortexa9_0} -display-name {freertos10_xilinx_ps7_cortexa9_0} -os {freertos10_xilinx} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {freertos_lwip_udp_perf_server}
platform generate -domains 
platform active {design_1_wrapper}
domain active {zynq_fsbl}
domain active {freertos10_xilinx_ps7_cortexa9_0}
platform generate -quick
platform generate