
# Packervisor Loadbalancer
Simple loadbalancer for packetvisor

# How to use
## 1. Set loadbalancer configuration file
```
<loadbalancer>
	<service> <!-- Service -->
		<address>192.168.0.200</address> <!-- Service IPv4 address -->
		<protocol>udp</protocol> <!-- Service protocol -->
		<port>7</port> <!-- Service port number -->
		<schedule>rr</schedule> <!-- Scheduling algorith: rr(Round-Robine) -->
		<servers> <!-- Server list -->
			<server>
				<address>10.0.0.200</address> <!-- Server IPv4 address -->
				<port>8080</port> <!-- Server port -->
				<mode>nat</mode> <!-- Translation mode -->
			</server>
			<server>
				<address>10.0.0.200</address> <!-- Server IPv4 address -->
				<port>8081</port> <!-- Server port -->
				<mode>nat</mode> <!-- Translation mode -->
			</server>
			<server>
				<address>10.0.0.200</address>
				<port>8082</port>
				<mode>nat</mode>
			</server>
		</servers>
	</service>
</loadbalancer>
```

## 2. Set network interface address & loadbalancer configuration path to packetlet parameter
```
<packetvisor>
	<umem>
			<frames>4096</frames>
	</umem>
	<xdp dev="veth0">
		<mac>06:d5:70:51:5b:78</mac>
		<pcap>/tmp/pv</pcap>
		<mode>auto</mode> <!-- auto, skb, native, offload -->
		<polling>false</polling>
		<copy>copy</copy> <!-- copy, zero -->
		<program>pv.o</program>
		<section>xdp_sock</section>
		<queue>0</queue>
		<packetlet>
			<path>examples/loadbalancer/loadbalancer.so</path>
			<arg>192.168.0.100</arg> <!-- IPv4 address -->
			<arg>255.255.255.0</arg> <!-- Netmask -->
			<arg>192.168.0.254</arg> <!-- Gateway address -->
			<arg>examples/loadbalancer/lb_config.xml</arg> <!-- Loadbalancer configuration file path -->
		</packetlet>
	</xdp>
	<xdp dev="veth1">
		<mac>4a:6c:7a:2c:cd:af</mac>
		<pcap>/tmp/pv2</pcap>
		<mode>auto</mode> <!-- auto, skb, native, offload -->
		<polling>false</polling>
		<copy>copy</copy> <!-- copy, zero -->
		<program>pv.o</program>
		<section>xdp_sock</section>
		<queue>0</queue>
		<packetlet>
			<path>examples/loadbalancer/loadbalancer.so</path>
			<arg>10.0.0.10</arg> <!-- IPv4 address -->
			<arg>255.255.255.0</arg> <!-- Netmask -->
			<arg>10.0.0.254</arg> <!-- Gateway address -->
		</packetlet>
	</xdp>
</packetvisor>
```

## 3. Start example

Execute loadbalancer
```
$ sudo ./testenv/testenv.sh setup --name veth0
$ sudo ./testenv/testenv.sh setup --name veth1
$ make loadbalancer
```

Set your server ip address
```
$ sudo ./testenv/testenv.sh enter --name veth1
# ip addr add 10.0.0.200/24 dev veth0 /* note that after enter dev name is always veth0 */
# route add default gw 10.0.0.10
```

Set your client ip address
```
$ sudo ./testenv/testenv.sh enter --name veth0
# ip addr add 192.168.0.10/24 dev veth0
```

 Run server1
```
$ sudo ./testenv/testenv.sh enter --name veth1
# nc -l -u -p 8080
```

 Run server2(optional)
```
$ sudo ./testenv/testenv.sh enter --name veth1
# nc -l -u -p 8081
```

Run server3(optional)
```
$ sudo ./testenv/testenv.sh enter --name veth1
# nc -l -u -p 8082
```

Run client
```
$ sudo ./testenv/testenv.sh enter --name veth0
# nc -u 192.168.0.200 8080
```

# TODO list
- [X] NAT(Network Address Translation)
- [ ] SNAT(Source Network Adress Translation)
- [ ] DR(Direc-Routing)
- [X] RR(Round-Robin)
- [ ] WRR(Weighted Round-Robin)
- [ ] LC(Least Connection)
- [ ] WLC(Weighted Least Connection)
