#!/bin/sh

FW="/sbin/iptables"

# vorhandene Regeln & Ketten löschen
$FW -F
$FW -X

# Default Regeln allgemein
$FW -P INPUT DROP
$FW -P FORWARD DROP
$FW -P OUTPUT DROP

# Eingänge freigeben
$FW -A INPUT -i lo -j ACCEPT
$FW -A INPUT -i eth0 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT	# Bestehende Verbindungen
$FW -A INPUT -i eth0 -p icmp --icmp-type 8 --src 192.168.1.0/24 -j ACCEPT # Ping
$FW -A INPUT -i eth0 -p tcp --src 192.168.1.0/24 --dport 22 -j ACCEPT # SSH
$FW -A INPUT -i eth0 -p udp --src 192.168.1.0/24 --dport 123 -j ACCEPT # NTP
$FW -A INPUT -j REJECT

# Ausgänge freigeben
$FW -A OUTPUT -o lo -j ACCEPT
$FW -A OUTPUT -o eth0 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT	# Bestehende Verbindungen
$FW -A OUTPUT -o eth0 -p tcp --dport 80 -j ACCEPT	# HTTP (für Update)
$FW -A OUTPUT -o eth0 -p tcp --dport 443 -j ACCEPT	# HTTPS (für Github)
$FW -A OUTPUT -o eth0 -p udp --dport 53 -j ACCEPT	# Domain
$FW -A OUTPUT -o eth0 -p udp --dport 123 -j ACCEPT	# NTP
$FW -A OUTPUT -j REJECT
