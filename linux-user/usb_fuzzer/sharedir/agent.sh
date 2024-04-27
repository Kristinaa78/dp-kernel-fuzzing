#!/bin/sh

>&2 echo "[i] agent.sh starting" | vmcall hcat

>&2 echo "[i] downloading fuzzer" | vmcall hcat
vmcall hget -x -o /fuzz fuzzer
cd /fuzz
>&2 echo "[+] starting the fuzzing process" | vmcall hcat
fuzzer

# return to loader.sh, which will upload agent.log
