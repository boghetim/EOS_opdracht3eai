# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct D:\Embedded_Systemen\HW_EOS\eind2\design_1_wrapper_1\platform.tcl
# 
# OR launch xsct and run below command.
# source D:\Embedded_Systemen\HW_EOS\eind2\design_1_wrapper_1\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {design_1_wrapper_1}\
-hw {D:\EOS_opdracht3eai\vraag2\vitis_audio\design_1_wrapper.xsa}\
-out {D:/Embedded_Systemen/HW_EOS/eind2}

platform write
domain create -name {freertos10_xilinx_ps7_cortexa9_0} -display-name {freertos10_xilinx_ps7_cortexa9_0} -os {freertos10_xilinx} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {freertos_lwip_udp_perf_server}
platform generate -domains 
platform active {design_1_wrapper_1}
domain active {zynq_fsbl}
domain active {freertos10_xilinx_ps7_cortexa9_0}
platform generate -quick
catch {platform remove design_1_wrapper_1}
platform create -name {design_1_wrapper_1}\
-hw {D:\EOS_opdracht3eai\vraag2\vitis_audio\design_1_wrapper.xsa}\
-out {D:/Embedded_Systemen/HW_EOS/eind2}

platform write
domain create -name {freertos10_xilinx_ps7_cortexa9_0} -display-name {freertos10_xilinx_ps7_cortexa9_0} -os {freertos10_xilinx} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {freertos_hello_world}
platform generate -domains 
platform write
domain active {zynq_fsbl}
domain active {freertos10_xilinx_ps7_cortexa9_0}
platform generate -quick
