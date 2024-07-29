## Summary
Filter: TCP packet filtering between two network interfaces

## Run example
To execute filter example.

```sh
# set veths
$ sudo ./set_veth.sh
# run example
$ sudo ./target/release/examples/filter --nic1 veth0 --nic2 veth2

# Run test server 
$ sudo ip netns exec test2 nc -l 0.0.0.0 8080 -u
# Run test client 
$ sudo ip netns exec test1 nc -u 10.0.0.7 8080
```
Then, communicate between server and client.

To remove veths created by `set_veth.sh`, `unset_veth.sh` will remove them.
