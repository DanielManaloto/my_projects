# 🖥️ LC-3 Virtual Machine (Python)

A simple LC-3 (Little Computer 3) virtual machine implemented in Python.  
This project emulates the LC-3 architecture, including registers, memory, 
instruction set, and trap routines.

---

## ✨ Features

- Implements full LC-3 instruction set  
- Supports memory-mapped I/O  
- Handles condition flags (N, Z, P)  
- Supports TRAP routines (GETC, OUT, PUTS, IN, PUTSP, HALT)  
- Loads and executes LC-3 binary image files  
- Simulates 16-bit memory and registers  

---

## 🧪 Inspiration / Reference

This project is based on and inspired by the LC-3 VM tutorial:

- https://www.jmeiners.com/lc3-vm/

The tutorial explains how to build a virtual machine that simulates a CPU, 
including memory, registers, and instruction execution.

---

## 🛠️ Requirements

- Python 3.x  
- Windows (uses `msvcrt` for keyboard input)

---

## ▶️ Usage

```bash
python lc3.py <image-file>
