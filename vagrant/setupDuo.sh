#!/bin/bash

#This script assumes that the following environment variables are set:
#DUO_IKEY
#DUO_SKEY
#DUO_APIHOST
#This script was written for Ubuntu 14.x
echo "Setting up Duo"
apt-get -y install libssl-dev libpam-dev
cd /vagrant
wget https://dl.duosecurity.com/duo_unix-latest.tar.gz
tar zxf duo_unix-latest.tar.gz
#since we don't know the verison number
cd duo_unix-*
./configure --with-pam --prefix=/usr && make && sudo make install	

#clean up
rm -rf /vagrant/duo_unix-*

echo "setting up pam_duo.conf"
#update configuration file with values provided by vagrant_vars.rb
sed -i s/"ikey = "/"&$DUO_IKEY"/g /etc/duo/pam_duo.conf
sed -i s/"skey = "/"&$DUO_SKEY"/g /etc/duo/pam_duo.conf
sed -i s/"host = "/"&$DUO_APIHOST"/g /etc/duo/pam_duo.conf

#turn on duo for PAM auth (SSH Public Key Authentication)
echo "setting up /etc/pam.d/sshd"		
sed -i s/"@include common-auth"/"#@include common-auth\\
auth  [success=1 default=ignore] \/lib64\/security\/pam_duo.so\\
auth  requisite pam_deny.so\\
auth  required pam_permit.so\\
auth  optional pam_cap.so"/g /etc/pam.d/sshd

#System-wide Authentication
echo "commenting out content in /etc/pam.d/common-auth"
sed -i s/"auth[[:space:]]\+\[success=1 default=ignore\][[:space:]]\+pam_unix.so nullok_secure"/"#auth  [success=1 default=ignore] pam_unix.so nullok_secure"/g /etc/pam.d/common-auth

echo "inserting content in /etc/pam.d/common-auth"
sed -i s/"auth[[:space:]]\+requisite[[:space:]]\+pam_deny.so"/"auth  requisite pam_unix.so nullok_secure\\
auth  [success=1 default=ignore] \/lib64\/security\/pam_duo.so\\
&"/g /etc/pam.d/common-auth

#enable Duo
echo "enabling duo in /etc/ssh/sshd_config"
sed -i s/"ChallengeResponseAuthentication no"/"ChallengeResponseAuthentication yes"/g /etc/ssh/sshd_config

#sudo chmod 600 /etc/duo/pam_duo.conf
#restart SSH service
service ssh restart