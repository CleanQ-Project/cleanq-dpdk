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

### Bulding DPDK

```bash
cd dpdk-stable-18.11.1 
make config install T=x86_64-native-linuxapp-gcc 
```

In both x86_64-native-linuxapp-gcc/.config and config/common_linuxapp add
CONFIG_RTE_LIBCLEANQ=y

then recompile

```bash
make T=x86_64-native-linuxapp-gcc 
```

To run DPDK withouth CleanQ for comparison simply remove the CONFIG_RTE_LIBCLEANQ

### Build applications

```bash
export RTE_SDK=< path to dpdk-stable-18.11.1>
cd benchmark_cleanq_udp
make
cd ../udp_bench
make
```


### Setup DPDK server

```bash
bash usertools/dpdk-setup.sh
```

Press 21/22 to set up hugepage mappings for non-NUMA or NUMA systems

Press 18 to insert IGB UIO module. 

Press 24 to bind device to IGB UIO module

### Setup UDP client

Since there is no ARP implementation on the server side, ARP entries
have to be added and some additional setup is required

```bash
ifconfig <if_name> up
ifconfig <if_name> <client_ip> netmask 255.255.252.0
ip route add <server_ip> dev <if_name>
arp -s <server_ip> <server_mac> -i <if_name>
```

### Start DPDK server

The application (benchmark_cleanq_udp) is rather crude and we did not implement
ARP functionality. Consequently src/dst IP and destination MAC are hardcoded in
the application. 

1. Adapt src_ip_str/dst_ip_str and dst_mac accordingly
2. define/undefine #CLEANQ_STACK. if CLEANQ_STACK is defined the small UDP stack 
   is used instead of the DPDK echo implementation. The CleanQ stack only works
   in combination with DPDK compiled with CleanQ enabled. 

recompile (make) the application and run it. 

### Start UDP client 

The client application has many different parameters
```bash
 ./udp_bench <num_clients> <if_name> <port_nr> <server_ip> <run_time> <pkts_in_flight> <buf_size> <rounds> <outfile>
```

 * num_clients: Number of clients. Each client will be assigned a core in a round 
   robin fashion
 * if_name: Name of the interface which should be used e.g enp94s0f0
 * port_nr: Port number to send to 
 * server_ip: Sever IP to send to
 * run_time: Run time in seconds for one run
 * pkts_in_flight: Number of concurrent packets in flight per client
 * buf_size: Buffer size used
 * rounds: Number of runs e.g. number of repetitions of the benchmark
 * outfile: Output file

