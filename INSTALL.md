# How to prepare Packetvisor development environment
This is how to prepare Packetvisor developing environment
based on Debian 10 + backports

## Install Debian 10
[Install VirtualBox]
[Install Debian 10]

## Set an user as a sudoer
su -
[enter root password]
$ usermod -aG sudo [username]

logout and login again

## Install Linux kernel 5.4.x
[add below line to /etc/apt/sources.list]
deb http://deb.debian.org/debian buster-backports main

sudo apt update
sudo apt upgrade
sudo apt install linux-image-5.4.0-0.bpo.2-amd64
sudo reboot

## Install VirtualBox guest addition
sudo apt install build-essential module-assistant
sudo m-a prepare
[Insert cdrom]
sudo sh /media/cdrom0/VBoxLinuxAdditions.run
sudo reboot

## Prepare for git clone
sudo apt install vim git

## Prepare dependent libraries
sudo apt install clang llvm gcc-multilib libelf-dev libpugixml-dev libtbb-dev
sudo apt install ethtool

## git clone
git clone https://github.com/semihlab/packetvisor
cd packetvisor
git submodule init
git submodule update

# echo server (Terminal #2)
make setup
make run

# echo client (Terminal #1)
make enter
ip addr add 192.168.0.10/24 dev veth0
ping 192.18.0.1 -c 3

# Other configurations
## git options
git config --global user.email "[your email]"
git config --global user.name "[your name]"
git config --global core.editor "vim"

## .vimrc options
:set ts=4
:set sw=4
:set ai
