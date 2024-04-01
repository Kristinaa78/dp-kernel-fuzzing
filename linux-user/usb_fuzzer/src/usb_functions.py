"""
usb_functions.py serves to retrieve kernel function addresses from a statically linked Linux kernel
binary (vmlinux). it uses nm utility to load all symbolic information from the binary. iteratively, each
kernel function specified in the file usb_functions.txt is searched for within the nm's output. this script
writes the result in the output file ('functions_addresses.txt'), making it available for further processing.

- kristina hrebenarova
- 2024
"""
import subprocess

# kernel needs to be compiled with all debug information (CONFIG_DEBUG_INFO=y)
vmlinux = "../../linux_kafl_agent/vmlinux"
# list of functions has been obtained from the kernel documentation
# at [https://www.kernel.org/doc/html/latest/driver-api/usb/usb.html] 
functions = "usb_functions.txt"

"""
function runs the nm utility against the vmlinux binary and saves the parsed output in a list
"""
def get_symbols():
    try:
        result = subprocess.run(["nm", vmlinux], capture_output=True, text=True, check=True)
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
        # as in: [ffffffff82cbace0 T usb_find_alt_setting]
        if len(data) >= 3 and function == data[2]:
            return data[0]
    return None

"""
function searches each function name in the nm's output. not every function mentioned in the file
is retrievable with the nm utility (as they might not be exported -- such as static functions). however,
this approach sufficiently provides the IP range information as a whole. it returns a dictionary
of individual the <function name, address> pairs.
"""
def search_addresses():
    function_addresses = {}
    symbols = get_symbols()

    with open(functions, 'r') as file:
        for function in file:
            function = function.strip()
            address = find_address(function, symbols)
            if address:
                function_addresses[function] = address
            else:
                print(f"{function} not found")

    return function_addresses

"""
function sorts the whole dictionary by the addresses.
"""
def sort_dictionary(function_addresses):
    sorted_items = sorted(function_addresses.items(), key=lambda item: int(item[1], 16))
    sorted_function_addresses = dict(sorted_items)

    return sorted_function_addresses

if __name__ == "__main__":
    addresses_dict = search_addresses()
    sorted_dict = sort_dictionary(addresses_dict)
    with open("functions_addresses.txt", 'w') as file:
        for function, address in sorted_dict.items():
            file.write(f"0x{address}: {function}\n")

    addresses = [int(address, 16) for address in sorted_dict.values()]
    # print the whole IP range
    print(f"Range: {hex(min(addresses))} - {hex(max(addresses))}")
