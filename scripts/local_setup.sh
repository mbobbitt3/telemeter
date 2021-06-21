#!/bin/bash

# Run with the IP address of your Equinix instances as the only parameter, e.g.,
#   remote_setup.sh 139.178.91.233

# Copy useful configuration files from the local home directory to the Equinix server.
scp -i .ssh/equinix/id_rsa .tmux.conf .vimrc .gitconfig .inputrc ${1}:

# Copy the github private rsa key to the equinix server.
# Best to get a firewall up and running before copying this over.
scp -i .ssh/equinix/id_rsa .ssh/github/id_rsa ${1}:.ssh/github/id_rsa