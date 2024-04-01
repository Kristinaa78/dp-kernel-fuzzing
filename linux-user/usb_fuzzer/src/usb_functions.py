import subprocess

vmlinux = "../../linux_kafl_agent/vmlinux"
functions = "usb_functions.txt"

def get_symbols():
    try:
        result = subprocess.run(["nm", vmlinux], capture_output=True, text=True, check=True)
        return result.stdout.splitlines()
    except subprocess.CalledProcessError as e:
        print(f"Error: {e}")
        return []

def find_address(function, symbols):
    for line in symbols:
        data = line.split()
        # as in: [ffffffff82cbace0 T usb_find_alt_setting]
        if len(data) >= 3 and function == data[2]:
            return data[0]
    return None

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
    min_address = min(addresses)
    max_address = max(addresses)
    print(f"Range: {hex(min_address)} - {hex(max_address)}")
