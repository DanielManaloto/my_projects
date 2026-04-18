# 🖼️ BMP Viewer (C + SDL2)

A simple BMP image viewer written in C using SDL2.  
This project focuses on parsing and rendering BMP files with support for multiple formats and compression types.

---

## ✨ Features

- Supports Windows and OS/2 BMP formats  
- Handles 1, 4, 8, 16, 24, and 32-bit images  
- Supports RLE4 and RLE8 compression  
- Handles palette-based images  
- Supports bitfield color masks  
- Converts all formats to 32-bit BGRA for rendering  
- Displays images using SDL2  

---

## 🧪 Testing

Tested using BMP images from:  
https://entropymine.com/jason/bmpsuite/bmpsuite/html/bmpsuite.html  

---

## 🛠️ Build

```bash
gcc main.c -o bmp_viewer -lSDL2
