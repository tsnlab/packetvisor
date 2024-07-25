## Example
- echo : ARP, ICMP, UDP echo server

## Run example
echo example has many options. If you want to see the options.
Execute `echo --help`

To execute echo example.

Execute `sudo ./set_veth.sh` then execute `sudo ./echo veth0` in `target/release/examples/`.

echo example has three functions, ARP reply, ICMP echo(ping), UDP echo.

If you want to test ARP reply, Execute the following command.
`sudo ip netns exec test_namespace arping 10.0.0.4`.

If you want to test ICMP echo, Execute the following command.
`sudo ip netns exec test_namespace ping 10.0.0.4`.
In case of ipv6, Execute the following command.
`sudo ip netns exec test_namespace ping 2001:db8::1`.


If you want to test UDP echo, Execute the following command.
`sudo ip netns exec test_namespace nc -u 10.0.0.4 7`
In case of ipv6, Execute the following command.
`sudo ip netns exec test_namespace nc -u 2001:db8::1 7`
