"""
usb_functions.py serves to retrieve kernel function addresses from a statically linked Linux kernel
binary (vmlinux). it uses nm utility to load all symbolic information from the binary. iteratively, each
kernel function specified in the file usb_functions.txt is searched for within the nm's output. this script
writes the result in the output file ('functions_addresses.txt'), making it available for further processing.

author: kristina hrebenarova
created: 03/2024
"""

import subprocess
import argparse
import sys

"""
function runs the nm utility against the vmlinux binary and saves the parsed output in a list
"""
def get_symbols():
    try:
        result = subprocess.run(["nm", "--print-size", vmlinux], capture_output=True, text=True, check=True)
        return result.stdout.splitlines()
    except subprocess.CalledProcessError as e:
        print(f"Error: {e}")
        return []

"""
function parses an individual line of the nm's output.
"""
def find_address(function, symbols):
    for line in symbols:
        data = line.split()
        # as in: [ffffffff82cf2d00 00000000000007d1 t usb_unbind_interface]
        if len(data) >= 4 and function == data[3]:
            return data[0], data[1]
    return None, None

"""
function searches each function name in the nm's output. not every function mentioned in the file
is retrievable with the nm utility (as they might not be exported -- such as static functions). however,
this approach sufficiently provides the IP range information as a whole. it returns a dictionary
of individual the <function name, address> pairs.
"""
def search_addresses():
    function_addresses = {}
    function_sizes = {}
    symbols = get_symbols()

    with open(file_in, 'r') as file:
        for function in file:
            function = function.strip()
            address, size = find_address(function, symbols)
            if address:
                function_addresses[function] = address
                function_sizes[function] = size
            else:
                print(f"{function} not found")

    return function_addresses, function_sizes

"""
function sorts the whole dictionary by the addresses.
"""
def sort_dictionary(function_addresses):
    sorted_items = sorted(function_addresses.items(), key=lambda item: int(item[1], 16))
    sorted_function_addresses = dict(sorted_items)

    return sorted_function_addresses

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=
        "program determines an IP range for functions specified in the <input> file via nm utility.")
    parser.add_argument('--input', type=str, required=False, help="file with functions to search for in symbols")
    parser.add_argument('--output', type=str, required=False, help='file to save the results in')
    parser.add_argument('--vmlinux', type=str, required=False, help='path to vmlinux binary')
    args = parser.parse_args()

    # list of functions has been obtained from the kernel documentation
    # at [https://www.kernel.org/doc/html/latest/driver-api/usb/usb.html]
    file_in  = args.input if args.input else "usb_functions.txt"
    file_out = args.output if args.output else "usb_addresses.txt"
     # kernel needs to be compiled with all debug information (CONFIG_DEBUG_INFO=y)
    vmlinux  = args.vmlinux if args.vmlinux else "../../../linux_kafl_agent/vmlinux"

    addresses_dict, sizes_dict = search_addresses()
    sorted_dict = sort_dictionary(addresses_dict)
    with open(file_out, 'w') as file:
        for function, address in sorted_dict.items():
            file.write(f"0x{address}: {function}\n")

    addresses = [int(address, 16) for address in sorted_dict.values()]
    last = list(sorted_dict) [-1]
    last_addr = max(addresses) + int(sizes_dict[last], 16)
    # print the whole IP range
    print("----------------------------------------------")
    print(f"range: {hex(min(addresses))} - {hex(last_addr)}")
    print(f"data written to {file_out}")

