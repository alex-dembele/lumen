#!/usr/bin/env python3
"""Affiche les octets d'un eboot.bin à l'adresse d'un plantage signalé dans un log shadPS4.

Usage : inspect_crash.py <log shadPS4> <eboot.bin>

Le script lit dans le log l'adresse de chargement de eboot.bin et l'adresse du plantage, convertit
cette adresse en position dans le fichier ELF et affiche les octets autour. Il signale notamment
« CD 41 » (int 0x41), l'instruction par laquelle les jeux Naughty Dog terminent leurs asserts.
"""

import re
import struct
import sys
from pathlib import Path


def find_addresses(log_text):
    base = re.search(r"Loading module eboot\.bin to (0x[0-9a-fA-F]+)", log_text)
    crash = re.findall(r"(?:Unhandled Exception code 0x[0-9a-fA-F]+ at|code address) (0x[0-9a-fA-F]+)",
                       log_text)
    if not base or not crash:
        sys.exit("Adresse de chargement ou de plantage introuvable dans le log.")
    return int(base.group(1), 16), int(crash[-1], 16)


def vaddr_to_offset(elf, vaddr):
    if elf[:4] != b"\x7fELF":
        sys.exit("eboot.bin n'est pas un ELF déchiffré.")
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    for i in range(phnum):
        p_type, _, p_offset, p_vaddr, _, p_filesz, _ = struct.unpack_from(
            "<IIQQQQQ", elf, phoff + i * phentsize)
        if p_vaddr <= vaddr < p_vaddr + p_filesz:
            return p_offset + (vaddr - p_vaddr)
    return None


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    log_text = Path(sys.argv[1]).read_text(errors="replace")
    elf = Path(sys.argv[2]).read_bytes()

    base, crash = find_addresses(log_text)
    vaddr = crash - base
    print(f"eboot.bin chargé à {base:#x}, plantage à {crash:#x} -> adresse ELF {vaddr:#x}")

    offset = vaddr_to_offset(elf, vaddr)
    if offset is None:
        sys.exit("Cette adresse n'est pas dans eboot.bin (plantage dans une bibliothèque système ?).")

    before = elf[max(0, offset - 8):offset]
    at = elf[offset:offset + 8]
    print("octets avant :", before.hex(" "))
    print("octets à l'adresse :", at.hex(" "))
    if at[:2] == b"\xcd\x41":
        print("=> int 0x41 : assert Naughty Dog. Le jeu signale une erreur interne à cet endroit.")
    else:
        print("=> pas un int 0x41 : vrai accès mémoire invalide, à analyser avec un désassembleur.")


if __name__ == "__main__":
    main()
