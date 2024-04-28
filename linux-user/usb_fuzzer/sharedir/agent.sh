#!/bin/sh

>&2 echo "[i] agent.sh starting" | vmcall hcat

# download the USB fuzzing device from host
>&2 echo "[i] downloading fuzzer" | vmcall hcat
vmcall hget -x -o /fuzz fuzzer
cd /fuzz

# check if kaslr is not set up
>&2 echo "[i] cat /proc/kallsyms | grep 'usb_free_coherent':" | vmcall hcat
cat /proc/kallsyms | grep "usb_free_coherent"

>&2 echo "[+] starting the fuzzing process" | vmcall hcat
fuzzer

# return to loader.sh, which will upload agent.log
