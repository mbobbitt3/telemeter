#!/bin/bash 

# To use:
# 1. Log into your new Equinix Metal instance as root, e.g.,
#	ssh -i ./.ssh/equinix/id_rsa root@www.xxx.yyy.zzz
# 2. On github, go to https://gist.github.com/rountree/4910d7bda0e6d8fd3b7c3537c76ea188
#	and click on the "Raw" button.
# 3. Copy the URL, e.g.,
#	https://gist.githubusercontent.com/rountree/4910d7bda0e6d8fd3b7c3537c76ea188/raw/ae1523aaaa68b51719a7bf39491b8b0695daf66d/equinix_setup.sh
# 4. Back on Equinix, past the URL into the following command:
#	wget <paste raw url here>
# 5. Edit the file to change the MY_USERNAME variable to something appropriate. 
# 6. Edit the packages list as needed.
# 6. Make the file executable:
#	chmod 700 equinix_setup.sh
# 7. Run the file
# 	./equinix_setup.sh
  
# Sets up an Equinix Metal instance for development.
export MY_USERNAME=rountree
export PACKAGES="manpages manpages-dev manpages-posix manpages-posix-dev vim git tmux msr-tools time apt-file \
	build-essential libhugetlbfs-bin libhugetlbfs-dev libhugetlbfs0 clang-10 r-base libomp-11-doc \
	libomp-11-dev libssl-dev libssl-doc numactl"

# removed openssl


###################################################################
#
# For most cases you should not need to modify anything below 
#
###################################################################


# Add source repos to apt
sed -i -e "s/# deb/deb/" /etc/apt/sources.list

# Remove cloud-init as it wants to be interactive when upgraded
DEBIAN_FRONTEND=noninteractive apt-get --assume-yes remove cloud-init

# Do an update (but no upgrade yet)
DEBIAN_FRONTEND=noninteractive apt-get --assume-yes update

# Set up firewall sooner to only allow incoming ssh.
DEBIAN_FRONTEND=noninteractive apt-get --assume-yes install ufw
ufw default deny incoming
ufw default allow outgoing
ufw allow ssh
ufw --force enable

# BEGIN UNMINIMIZE (minus the interactive bits)
if [ -f /etc/dpkg/dpkg.cfg.d/excludes ] || [ -f /etc/dpkg/dpkg.cfg.d/excludes.dpkg-tmp ]; then
    echo "Re-enabling installation of all documentation in dpkg..."
    if [ -f /etc/dpkg/dpkg.cfg.d/excludes ]; then
        mv /etc/dpkg/dpkg.cfg.d/excludes /etc/dpkg/dpkg.cfg.d/excludes.dpkg-tmp
    fi

    echo "Restoring system documentation..."
    echo "Reinstalling packages with files in /usr/share/man/ ..."
    # Reinstallation takes place in two steps because a single dpkg --verified
    # command generates very long parameter list for "xargs dpkg -S" and may go
    # over ARG_MAX. Since many packages have man pages the second download
    # handles a much smaller amount of packages.
    dpkg -S /usr/share/man/ |sed 's|, |\n|g;s|: [^:]*$||' | DEBIAN_FRONTEND=noninteractive xargs apt-get install --reinstall -y
    echo "Reinstalling packages with system documentation in /usr/share/doc/ .."
    # This step processes the packages which still have missing documentation
    dpkg --verify --verify-format rpm | awk '/..5......   \/usr\/share\/doc/ {print $2}' | sed 's|/[^/]*$||' | sort |uniq \
         | xargs dpkg -S | sed 's|, |\n|g;s|: [^:]*$||' | uniq | DEBIAN_FRONTEND=noninteractive xargs apt-get install --reinstall -y
    echo "Restoring system translations..."
    # This step processes the packages which still have missing translations
    dpkg --verify --verify-format rpm | awk '/..5......   \/usr\/share\/locale/ {print $2}' | sed 's|/[^/]*$||' | sort |uniq \
         | xargs dpkg -S | sed 's|, |\n|g;s|: [^:]*$||' | uniq | DEBIAN_FRONTEND=noninteractive xargs apt-get install --reinstall -y
    if dpkg --verify --verify-format rpm | awk '/..5......   \/usr\/share\/doc/ {exit 1}'; then
        echo "Documentation has been restored successfully."
        rm /etc/dpkg/dpkg.cfg.d/excludes.dpkg-tmp
    else
        echo "There are still files missing from /usr/share/doc/:"
        dpkg --verify --verify-format rpm | awk '/..5......   \/usr\/share\/doc/ {print " " $2}'
        echo "You may want to try running this script again or you can remove"
        echo "/etc/dpkg/dpkg.cfg.d/excludes.dpkg-tmp and restore the files manually."
    fi
fi

if  [ "$(dpkg-divert --truename /usr/bin/man)" = "/usr/bin/man.REAL" ]; then
    # Remove diverted man binary
    rm -f /usr/bin/man
    dpkg-divert --quiet --remove --rename /usr/bin/man
fi

# unminimization succeeded, there is no need to mention it in motd
rm -f /etc/update-motd.d/60-unminimize

# END UNMINIMIZE 

# Now do the upgrade
DEBIAN_FRONTEND=noninteractive apt-get --assume-yes upgrade

# Install user-requested packages and whatever is needed to build them.
DEBIAN_FRONTEND=noninteractive apt-get --assume-yes install ${PACKAGES}
# DEBIAN_FRONTEND=noninteractive apt-get --assume-yes build-dep ${PACKAGES}


# Set up hugepages.  Requires a reboot to take effect.
# And turn off the Intel pstate driver while we're in the neighborhood.
# See https://www.kernel.org/doc/html/v4.12/admin-guide/pm/intel_pstate.html for details.
sed --in-place=.orig -e "s/splash/splash hugepagesz=1G hugepages=8 default_hugepagesz=1G intel_pstate=disable/" /etc/default/grub
grub-mkconfig -o /boot/grub/grub.cfg

# User creation
sed -i "s/%sudo\tALL=(ALL:ALL) ALL/%sudo\tALL=(ALL:ALL) NOPASSWD: ALL/" /etc/sudoers

adduser --disabled-password --gecos "" ${MY_USERNAME}
usermod -a -G sudo ${MY_USERNAME}
mkdir -p /home/${MY_USERNAME}/.ssh/github
cp ~/.ssh/authorized_keys /home/${MY_USERNAME}/.ssh
chmod 600 /home/${MY_USERNAME}/.ssh/authorized_keys
chmod 700 /home/${MY_USERNAME}/.ssh/
chmod 755 /home/${MY_USERNAME}/.ssh/github
chown -R ${MY_USERNAME}.${MY_USERNAME} /home/${MY_USERNAME}/.ssh

# Don't know why ~${MY_USERNAME} doesn't work here, but it doesn't.
sudo -u ${MY_USERNAME} \
	printf %b "Host github.com\n\tIdentityFile ~/.ssh/github/id_rsa\n" > \
	/home/${MY_USERNAME}/.ssh/config
	
su ${MY_USERNAME}
