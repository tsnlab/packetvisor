#!/usr/bin/env python3

import argparse
import socket
import sys

parser = argparse.ArgumentParser(description='UDP echo client')
parser.add_argument('host', help='host to connect')
parser.add_argument('port', type=int, help='port number to connect')
parser.add_argument(
    '-t', '--timeout', type=int, default=2,
    help='Timeout in seconds')
parser.add_argument(
    '-b', '--bind', type=int,
    help='If you want to set src port')

args = parser.parse_args()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
serv = (args.host, args.port)
sock.settimeout(args.timeout)

if args.bind:
    sock.bind(('0.0.0.0', args.bind))

while True:
    try:
        msg = input('send: ')
        sock.sendto(msg.encode('utf-8'), serv)
        data, server = sock.recvfrom(4096)
        print(f'recv: {data.decode("utf-8")}')
    except socket.timeout as e:
        print(e, file=sys.stderr)
        continue
    except (KeyboardInterrupt, EOFError):
        break
