## Example
- forward : Forward packets between two network interface

## Run example
To execute change_word example.

Execute `sudo ./set_veth.sh` then execute `sudo ./forward -- veth0 veth2` in `target/release/examples/`.

Run test server `sudo ip netns exec test2 nc -l 0.0.0.0 8080 -u`.
Run test client `sudo ip netns exec test1 nc -u 10.0.0.7 8080`.

Then, communicate between server and client.

To remove veths created by `set_veth.sh`, `unset_veth.sh` will remove them.
