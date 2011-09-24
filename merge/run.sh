#!/bin/bash
#cnet three_node_network -T -W -o trace%n -s
cnet -W -T -s -e 1min saarnet-3a.txt -o trace%n

#cnet -W -T -s -e5min saarnet-6.txt  -o trace%n

#cnet -W -T -s -e 15min saarnet-8a.txt -o trace%n

#cnet -W -T -s two_node_network -o trace%d
