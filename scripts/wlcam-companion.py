import sys
import os
import time
import threading
import subprocess
import signal

fifo_path = "/tmp/wlcam.fifo"

print("waiting for remote server")
while not os.path.exists(fifo_path):
    time.sleep(1)

file = open(fifo_path, "r")
fps  = 30

def worker(args, cwd):
    result = subprocess.run(args, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=cwd, executable="/bin/sh")
    if result.returncode != 0:
        print("command '{}' returned {}: ".format(args, result.returncode), result.stdout.decode('utf-8'))

def worker_async(args, cwd):
    return subprocess.Popen(args, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=cwd, executable="/bin/sh")

def handle_configure(args):
    for arg in args.split(" "):
        if(args.startswith("fps:")):
            fps = int(arg[4:])
        else:
           print("warn: unknown configure item:", arg)

record_process = None

def handle_record_start(tempdir):
    global record_process

    print("start \"{}\"".format(tempdir))
    args = "exec pw-record --target alsa_input.pci-0000_00_1f.3.analog-stereo '{}'".format(tempdir + "/audio.wav")
    record_process = worker_async(args, tempdir)

def handle_record_stop(tempdir):
    global record_process

    print("stop \"{}\"".format(tempdir))

    inputs = "-i concat.txt"
    if record_process != None:
        record_process.send_signal(signal.SIGINT)
        record_process = None
        inputs += " -i audio.wav"

    args = "ffmpeg -f concat {} -c:v h264_vaapi -vaapi_device /dev/dri/renderD128 -vf 'format=nv12,hwupload,fps={}' '../{}.mp4' && \
            cd / && \
            rm -rf '{}'" \
            .format(inputs, fps, os.path.basename(tempdir), tempdir)
    threading.Thread(target=worker, args=(args, tempdir)).start()

while True:
    line = file.readline()[:-1]
    if line.startswith("hello"):
        print("hello")
    elif line.startswith("configure"):
        handle_configure(line[line.find(" ") + 1:])
    elif line.startswith("record_start"):
        handle_record_start(line[line.find(" ") + 1:])
    elif line.startswith("record_stop"):
        handle_record_stop(line[line.find(" ") + 1:])
    elif line.startswith("bye"):
        print("bye")
        exit(0)
    else:
        print("warn: unknown command:", line)
