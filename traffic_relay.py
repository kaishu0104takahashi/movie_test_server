#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess
import datetime
import socket

# 車両側Clientの最新の4G固定IPアドレスを指定
CLIENT_4G_IP = "100.121.106.32" 
CLIENT_PORT = 50060

def get_local_traffic_gb(interface="eth0"):
    try:
        result = subprocess.check_output(["vnstat", "-i", interface, "-m"], text=True)
        current_month = datetime.datetime.now().strftime("%Y-%m")
        for line in result.split("\n"):
            if current_month in line:
                parts = line.split("|")
                if len(parts) >= 3:
                    total_str = parts[2].strip()
                    value_str, unit = total_str.split()
                    value = float(value_str)
                    if unit in ["KiB", "KB"]: return value / (1024 * 1024)
                    elif unit in ["MiB", "MB"]: return value / 1024
                    elif unit in ["GiB", "GB"]: return value
                    elif unit in ["TiB", "TB"]: return value * 1024
        return 0.0 
    except Exception:
        return -1.0

def get_remote_traffic_gb(ip, port):
    try:
        with socket.create_connection((ip, port), timeout=3.0) as s:
            data = s.recv(1024)
            if data: return float(data.decode('utf-8'))
    except Exception:
        return -2.0
    return -2.0

def main():
    HOST = '0.0.0.0'
    PORT = 50060 
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(1)
        while True:
            conn, addr = s.accept()
            with conn:
                svr_val = get_local_traffic_gb("eth0")
                cli_val = get_remote_traffic_gb(CLIENT_4G_IP, CLIENT_PORT)
                response = f"{svr_val},{cli_val}"
                conn.sendall(response.encode('utf-8'))

if __name__ == "__main__":
    main()