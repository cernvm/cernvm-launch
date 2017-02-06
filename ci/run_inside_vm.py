#!/usr/bin/env python

print("Running inside a VM, printing '/etc/issue'")

file = open('/etc/issue', 'r')
for line in file:
    print(line)

file.close()
