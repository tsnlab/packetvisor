## Examples
- echo : ARP, ICMP, UDP echo server
- forward : Forward packets between two network interface
- change_word : Find and replace some word from UDP flow while forwarding between two network interfaces
- filter : TCP packet filtering between two network interfaces
- tunnel : Tunneling packets received through packetvisor to standard udp socket

## Run examples
Example sources that show dealing with some network protocols using Packetvisor library are located in `examples/`.
To compile examples, run the following command: `cargo build -r --examples`.
They will be located in `target/release/examples/`

Example apps can be executed commonly like this: `sudo ./(EXAMPLE APP) <inferface name>`
Example apps also supports the help command like this: `./(EXAMPLE APP) --help`

You can test ipv4 and ipv6.

If you want to test ipv4, ipv6 you can use `set_veth.sh` to set veths for testing the application.
After the script is executed, veth0 and veth1 are created.
veth1 is created in `test_namespace` namespace but veth0 in local.

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

To execute change_word example.

Execute `sudo ./set_veth.sh`.
Execute change_word program `sudo ./target/release/examples/change_word --nic1 veth0 --nic2 veth2 --source-word apple --change-word watermelon`.
Run test server `sudo ip netns exec test2 nc -l 0.0.0.0 8080 -u`.
Run test client `sudo ip netns exec test1 nc -u 10.0.0.7 8080`.

Then, communicate between server and client.

To remove veths created by `set_veth.sh`, `unset_veth.sh` will remove them.
