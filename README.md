# CleanQ for Linux

This is the public repository of the CleanQ Linux implementation.
This implementation requires a Intel 82599 based NIC. 


## License

See the LICENSE file.


## Authors

Roni Haecki
Reto Achermann
Daniel Schwyn


## Contributing

See the CONTRIBUTING file.


## Dependencies

libnuma-dev

## Compiling and Running

**Bulding DPDK**

cd dpdk-stable-18.11.1 

make config install T=x86_64-native-linuxapp-gcc 

In both x86_64-native-linuxapp-gcc/.config and config/common_linuxapp add
CONFIG_RTE_LIBCLEANQ=y

then recompile

make T=x86_64-native-linuxapp-gcc 

To run DPDK withouth CleanQ for comparison simply remove the CONFIG_RTE_LIBCLEANQ

**Build applications**

export RTE_SDK=< path to dpdk-stable-18.11.1>

cd benchmark_cleanq_udp

make

**Setup DPDK**

bash usertools/dpdk-setup.sh

Press 21/22 to set up hugepage mappings for non-NUMA or NUMA systems

Press 18 to insert IGB UIO module. 

Press 24 to bind device to IGB UIO module

**Start application**

The application (benchmark_cleanq_udp) is rather crude and we did not implement
ARP functionality. Consequently src/dst IP and destination MAC are hardcoded in
the application. 

1. Adapt src_ip_str/dst_ip_str and dst_mac accordingly
2. define/undefine #CLEANQ_STACK. if CLEANQ_STACK is defined the small UDP stack 
   is used instead of the DPDK echo implementation. The CleanQ stack only works
   in combination with DPDK compiled with CleanQ enabled. 

recompile (make) the application and run it. 

