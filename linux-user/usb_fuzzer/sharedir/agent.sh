#!/bin/sh

>&2 echo "[i] agent.sh starting" | vmcall hcat
# print the contents of the root dir (debugging purposes)
ls -li /
# print the loader.sh contents
#>&2 cat /loader.sh
>&2 echo "[i] currently in: $(pwd)"
# set up USB testing device
# >&2 echo "[i] downloading sampleUSB" | vmcall hcat                                                               
# vmcall hget -x -o /fuzz sampleUSB
>&2 echo "[i] downloading fuzzer" | vmcall hcat
vmcall hget -x -o /fuzz fuzzer
cd /fuzz
# print the contents of the /fuzz dir (debugging purposes)
ls -li /fuzz
# >&2 echo "[+] starting the emulation" | vmcall hcat
# sampleUSB
>&2 echo "[+] starting the fuzzing process" | vmcall hcat
fuzzer
# return to loader.sh, which will upload agent.log
