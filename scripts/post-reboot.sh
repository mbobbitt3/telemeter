#!/bin/bash

# Run as a sudo-no-password user

# Set the clock frequency to as low as possible.
for i in /sys/devices/system/cpu/cpufreq/policy*/scaling_setspeed 
do 
  sudo chown rountree ${i}
  echo 800000 > ${i}
done
for i in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor
do 
  sudo chown rountree ${i}
  echo userspace > ${i}
done

sudo DEBIAN_FRONTEND=noninteractive apt-get --assume-yes install linux-headers-`uname -r`

cd
git clone git@github.com:rountree/msr-safe.git
cd msr-safe
git checkout -b rdmsr_enhancements origin/rdmsr_enhancements
make
sudo insmod ./msr-safe.ko
sudo modprobe msr
# In a shared development environment the group for the files below should be changed
# then run 
# chmod g+rw /dev/cpu/msr* /dev/cpu/msr/*/msr* 
sudo chown rountree /dev/cpu/msr* /dev/cpu/*/msr*
cat allowlists/platypus > /dev/cpu/msr_allowlist

cd
git clone git@github.com:rountree/telemeter.git
