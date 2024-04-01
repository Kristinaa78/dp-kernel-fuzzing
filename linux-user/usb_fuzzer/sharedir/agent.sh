#!/bin/sh

>&2 echo "[i] agent.sh starting" | vmcall hcat
# print loaded kernel modules (to fix 'UDC core: USB Raw Gadget: couldn't find an available UDC or it's busy')
lsmod
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
>&2 echo "[i] ls -li /fuzz:" | vmcall hcat
ls -li /fuzz
>&2 echo "[i] ls -li /dev:" | vmcall hcat
ls -li /dev

>&2 echo "[i] cat /proc/kallsyms | grep 'usb_free_coherent':" | vmcall hcat
cat /proc/kallsyms | grep "usb_free_coherent"

echo "Downloading dvkm.ko" | vmcall hcat
vmcall hget -x -o /fuzz dvkm.ko

echo "Inserting dvkm.ko" | vmcall hcat
insmod dvkm.ko


# >&2 echo "[+] starting the emulation" | vmcall hcat
# sampleUSB
>&2 echo "[+] starting the fuzzing process" | vmcall hcat
fuzzer
# return to loader.sh, which will upload agent.log
