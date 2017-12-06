# decode345 Vagrant setup
This set of files work along with Vagrant and VirtualBox to set up an Ubuntu environment that can serve as the foundation of your decode345 usage and development. 
The following will be installed in the Ubuntu Xenial VM:
- Docker
- OpenHAB2 (via Docker image)
- Mosquitto (via Docker image)
- Mosquitto clients
- gnuradio and dependencies
- SDR libraries (gr-osmosdr, librtlsdr0)
- decode345 source and dependencies
- decode345 binaries

## Requirements
1. Download and install Virtualbox (https://www.virtualbox.org/wiki/Downloads)
2. Download and install Vagrant (https://www.vagrantup.com/downloads.html)

## Setup
After downloading this folder (or cloning the repo), you'll need to modify the Vagrantfile and vagrant_vars.yaml for your setup.

### Vagrantfile
The following sections of the Vagrantfile should be modified, per your setup:

Mount the USB radio to the VM:
```
# Update the following variables below with information regarding your USB SDR device, and uncomment the 2 following lines:
# <DEVICE_NAME>: Arbitrary name for your device - this is a label that you will see when it is mounted to the VM.  Can be anthing you want.
# <VENDOR_ID>: To find the vendorid and productid, run `VBoxManage list usbhost` from the host machine
# <PRODUCT_ID>: To find the vendorid and productid, run `VBoxManage list usbhost` from the host machine
# If you do not see the USB device, install the VirtualBox Extension Pack (https://code-chronicle.blogspot.com/2014/08/connect-usb-device-through-vagrant.html)
      # vb.customize ["modifyvm", :id, "--usb", "on"]
      # vb.customize ['usbfilter', 'add', '0', '--target', :id, '--name', '<DEVICE_NAME>', '--vendorid', '<VENDOR_ID>', '--productid', '<PRODUCT_ID>']
```

Set permissions for Docker containers:
```
# Note that this will automatically bind to the host's network interface on ports 8080, 5555, and 8443, so those ports must be available. This is required for automatic device discovery (uPNP)
# Update the "USER_ID" value to match the user ID who runs this script. If YOU run this script, run `id` on the host to see your id. 
# Update the "device" value with the location of your SDR device inside the VM. The easiest way to see the location is to run (from within the VM) `ls /dev/tty*` copy the output, then unplug the device and run the command again. There should be one tty that disappears - that is where your device is mounted.
d.run "openhab", image: "openhab/openhab:2.1.0-amd64-debian", args: "--net host -v /etc/localtime:/etc/localtime:ro -v /etc/timezone:/etc/timezone:ro -d -v /vagrant/openhab/openhab_addons:/openhab/addons -v /vagrant/openhab/openhab_conf:/openhab/conf -v /vagrant/openhab/openhab_userdata:/openhab/userdata --restart=always -e USER_ID=1000 --device=/dev/ttyS0"
```
	
### vagrant_vars.yaml
The following sections of vagrant_vars.yaml should be modified, per your setup:

Set MAC Address for static IP assignment:
```
# This is used to provide a consistent MAC address to the guest VM. I use this to enable my router to provide a static IP.
guest_mac_addr:  "3333d02ffcf2" 
```

Set default gateway:
```
# Set to true to configure the guest to be able to communicate with your network (and be accessible via the Internet). The vagrant_gateway variable probably will not need
# to be changed, but the network_gateway variable should match the gateway you use for internal network communication. For many with 
# simple network configurations, this will be either 192.168.0.1 or 192.168.1.1.
networkAccess:  "true"
vagrant_gateway:  "10.0.2.2"
network_gateway: "192.168.1.1"
```

Set network interface:
```
#set this to the interface on the host machine that this VM will use for network access. On linux, you can see your host interfaces by typic 'ifconfig' 
host_interface: "eth0"
```

## Usage
1. Clone this repo - or just download the /vagrant folder
2. Open a command prompt (git bash is recommended, if using Windows: https://git-scm.com/downloads)
3. Through the command line, navigate to the folder containing this file (and more importantly, the Vagrantfile)
4. Execute: 
``` vagrant up ```
5. Wait for the VM to spin up and provision - it may take 10-15 minutes (or more depending on your network)
6. You'll need to open 3 TTYs (ssh terminals). If you're using Git Bash, just open 3 windows.  In each, navigate to the folder containing the Vagrantfile and type ```vagrant ssh```. Vagrant will handle the rest. If you're not using Git Bash, you'll probably want to use PuTTY for ssh. This process is more complicated (since you'll need to retrieve and use the SSH private key), and will not be documented here. 
7. In the first terminal, execute ```sudo python2.7 ~/decode345/bin/receive345.py```
8. In the second terminal, execute ```~/decode345/src/decode345 -f /tmp/grcfifo```
9. In the third terminal, execute ```sudo mosquitto_sub -d -t /security/#```
10. Open a web browser and navigate to ```https://<yourserversip>:8443``` and you should see openhab
