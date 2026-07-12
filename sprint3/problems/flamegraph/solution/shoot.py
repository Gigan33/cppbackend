import argparse
import os
import random
import shlex
import signal
import subprocess
import time

AMMUNITION = [
    'localhost:8080/api/v1/maps',
    'localhost:8080/api/v1/maps/map1',
]

def start_server(server_cmd):
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args([server_cmd]).server

def run(command):
    return subprocess.Popen(shlex.split(command), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def stop(process):
    process.terminate()
    process.wait()

def shoot(ammo):
    hit = f'curl {ammo}'
    subprocess.run(shlex.split(hit), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def make_shots():
    for _ in range(50):
        ammo = random.choice(AMMUNITION)
        shoot(ammo)
        time.sleep(0.1)

# --- Entry point ---

server_command = start_server(os.sys.argv[1])

print("Starting game server...")
server_proc = run(server_command)
time.sleep(0.5)

try:
    perf_command = f"perf record -g -p {server_proc.pid} -o perf.data"
    print(f"Starting perf record for PID {server_proc.pid}...")
    perf_proc = subprocess.Popen(shlex.split(perf_command), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.5)

    print("Shooting requests...")
    make_shots()

    print("Stopping perf record...")
    perf_proc.send_signal(signal.SIGINT)
    perf_proc.wait()

finally:
    print("Stopping game server...")
    stop(server_proc)

print("Generating graph.svg...")
try:
    perf_script_proc = subprocess.Popen(
        shlex.split("perf script -i perf.data"), 
        stdout=subprocess.PIPE, 
        stderr=subprocess.DEVNULL
    )

    collapse_proc = subprocess.Popen(
        shlex.split("./FlameGraph/stackcollapse-perf.pl"), 
        stdin=perf_script_proc.stdout, 
        stdout=subprocess.PIPE, 
        stderr=subprocess.DEVNULL
    )
    perf_script_proc.stdout.close()

    with open("graph.svg", "w") as svg_file:
        flamegraph_proc = subprocess.Popen(
            shlex.split("./FlameGraph/flamegraph.pl"), 
            stdin=collapse_proc.stdout, 
            stdout=svg_file, 
            stderr=subprocess.DEVNULL
        )
        collapse_proc.stdout.close()
        flamegraph_proc.wait()

    print("Success! graph.svg generated.")
except Exception as e:
    print(f"Error generating flamegraph: {e}")