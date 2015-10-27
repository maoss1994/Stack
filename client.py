#!/bin/env python3
#-*- coding: utf-8 -*-

import socket
import socks

SOCKS_PROXY_HOST = "127.0.0.1"
SOCKS_PROXY_PORT = 34567

socks.set_default_proxy(socks.SOCKS5, SOCKS_PROXY_HOST, SOCKS_PROXY_PORT)
socket.socket = socks.socksocket

import requests

print(requests.get("http://tools.ietf.org/html/rfc1928").text)

