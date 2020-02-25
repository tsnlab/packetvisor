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
	<xdp dev="veth0">
		<packetlet>
			<path>examples/loadbalancer/loadbalancer.so</path>
			<arg>192.168.0.100</arg> <!-- IPv4 address -->
			<arg>255.255.255.0</arg> <!-- Netmask -->
			<arg>192.168.0.254</arg> <!-- Gateway address -->
			<arg>examples/loadbalancer/lb_config.xml</arg> <!-- Loadbalancer configuration file path -->
		</packetlet>
	</xdp>
</packetvisor>
```

# TODO list
- [X] NAT(Network Address Translation)
- [ ] SNAT(Source Network Adress Translation)
- [ ] DR(Direc-Routing)
- [X] RR(Round-Robin)
- [ ] WRR(Weighted Round-Robin)
- [ ] LC(Least Connection)
- [ ] WLC(Weighted Least Connection)