#!/usr/bin/env python3
"""Write a large gtklab sessions file for list-view exercises.

usage: gen_sessions.py <count> <path>
"""
import sys

count = int(sys.argv[1])
path = sys.argv[2]
folders = ["Core", "Edge", "Peering", "DC1 Leaf", "DC1 Spine", "Lab"]
with open(path, "w") as f:
    f.write("[localhost]\nfolder=Local\nlocal=true\n\n")
    for i in range(count):
        folder = folders[i % len(folders)]
        name = f"{folder.lower().replace(' ', '-')}-{i:04d}"
        f.write(f"[{name}]\nfolder={folder}\nhost=10.{i // 250 % 250}.{i % 250}.1\nuser=admin\n")
        if i % 7 == 0:
            f.write("port=2222\n")
        f.write("\n")
