# 🪄🖼️ BMP Image Editor in C

A graphical image-manipulation application developed in **C99** using the **IUP toolkit**.

The application allows users to open, display, manipulate, undo, and save **24-bit uncompressed BMP images**. All image-processing algorithms are implemented manually in C by directly accessing and modifying dynamically allocated RGB pixel data.

---

## 👩‍💻 Author

- **Name:** Syeda Tasmiah Shariat
- **Course:** CSE1101L
- **Project:** Image Manipulation Software in C

---

## ⚙️ System Requirements

Ensure that the following tools and dependencies are available before building the project:

- **Operating System:** macOS on Apple Silicon
- **Programming Language:** C99
- **Compiler:** Apple Clang
- **GUI Toolkit:** IUP
- **Build Tool:** Make
- **Supported Image Format:** 24-bit uncompressed BMP
- **IUP Header Directory:** `/usr/local/include/iup`
- **IUP Library Directory:** `/usr/local/lib`

### Required IUP Header

The source code requires the main IUP header:

```text
iup.h
```

> **Note:** The included Makefile is configured and tested for macOS on Apple Silicon. The required IUP development headers and library files must be installed in the expected directories before compiling the project.

---

## 🚀 Setup & Installation

Follow these steps to build and run the application on macOS.

### 1. Download the Project

Download the repository as a ZIP file using the **Code** button on GitHub, then extract the archive.

Alternatively, clone the repository with Git if Git is available.

### 2. Open the Project Directory

Open Terminal in the extracted project directory, or navigate to it manually:

```bash
cd cse1101l-iup-bmp-image-editor
```

### 3. Verify the Project Files

The project directory should contain:

```text
src/
tests/
Makefile
README.md
```

### 4. Verify the Compiler

Confirm that Apple Clang is available:

```bash
clang --version
```

### 5. Verify the IUP Installation

Confirm that the IUP header and library files are available:

```bash
ls /usr/local/include/iup/iup.h
ls /usr/local/lib/libiup*
```

### 6. Remove Previous Build Files

```bash
make clean
```

### 7. Compile the Project

```bash
make
```

A successful build creates the following executable:

```text
image_editor
```

### 8. Run the Application

```bash
make run
```

### Build and Run Together

```bash
make clean && make && make run
```

### Run the Core Tests

```bash
make test
```

Expected output:

```text
All core tests passed.
```

> **Important:** The application supports only 24-bit uncompressed BMP images. PNG, JPEG, GIF, compressed BMP, and other image formats are not supported.

---

## 📌 Project Overview

This project demonstrates the practical application of structured programming concepts in C, including:

- Structures for representing pixels and images
- Arrays and pointer-based pixel access
- Dynamic memory allocation
- Modular source-code organization
- File handling for BMP images
- Manual image-processing algorithms
- Graphical user interface development with IUP
- Callback-based event handling
- Multi-level Undo using a linked-list stack
- Input validation and error handling
- Responsive image display
- Proper resource and memory cleanup

IUP is used for the graphical interface, layouts, canvas, dialogs, buttons, callbacks, and image presentation. The image-processing operations are implemented manually in C. The project does not call ready-made image-processing functions for grayscale conversion, brightness adjustment, inversion, flipping, rotation, cropping, blurring, or sharpening.

---

## ✨ Features

### 📂 File Operations

- Open an image using an IUP file dialog
- Accept only 24-bit uncompressed BMP images
- Validate the BMP signature, color depth, compression type, and dimensions
- Display an error dialog for invalid or unsupported files
- Save the edited image as a 24-bit uncompressed BMP
- Automatically add the `.bmp` extension when necessary
- Provide the following post-save actions:
  - Save and Exit
  - Keep Editing
  - Start New

### 🖼️ Image Manipulation

- Weighted grayscale conversion
- Brightness increase and decrease
- Image inversion
- Horizontal flip
- Vertical flip
- 90-degree clockwise rotation
- Rectangular crop
- 3 × 3 neighborhood blur
- Convolution-based image sharpening

### ↩️ Editing and Session Management

- Multi-level Undo
- Clear Undo History
- Unsaved Changes warning
- Start New confirmation
- Exit confirmation
- Image Information dialog
- Help dialog
- About dialog

### 🪟 Responsive Image Display

- Fits large images inside the available canvas
- Preserves the original aspect ratio
- Displays the image at its actual `1:1` size when sufficient space is available
- Never enlarges the image beyond its original resolution
- Rebuilds the preview whenever the window is resized
- Supports fullscreen resizing
- Keeps the image centered inside the canvas
- Preserves full-resolution image data for editing, Undo, and saving

### ⚠️ Large Image Warning

When an opened image would require more than approximately **64 MB for each full-resolution Undo snapshot**, the application displays a warning before continuing.

This warning helps users understand that:

- Every Undo state stores a deep copy of the image.
- Large images can consume significant memory.
- Repeated editing operations can increase total memory usage.
- The user may continue loading the image or cancel the operation.

---

## 🔍 Image Display Logic

The responsive display system follows this rule:

> **Fit down when necessary, display the actual size when enough space is available, and never upscale beyond the original resolution.**

The preview initially uses the original image dimensions:

```c
display_width = current_image->width;
display_height = current_image->height;
```

The available display area is calculated from the current canvas size:

```c
available_width = canvas_width - margin;
available_height = canvas_height - margin;
```

If the image does not fit inside the canvas, the program calculates two scaling ratios:

```text
Width ratio  = Available width / Original width
Height ratio = Available height / Original height
```

The smaller ratio is applied to both dimensions:

```text
Scale = minimum(Width ratio, Height ratio)

Display width  = Original width × Scale
Display height = Original height × Scale
```

This preserves the aspect ratio and ensures that the entire image remains visible.

### Example

For an original image of `1920 × 1280` pixels:

- A small canvas displays a proportionally reduced preview.
- A larger canvas displays a larger preview.
- A canvas with sufficient space displays the image at its actual `1:1` size.
- A canvas larger than the original image does not enlarge the image further.

Only the display preview is resized. The original image data remains at full resolution.

---

## 🧮 Image-Processing Algorithms

All image-processing algorithms operate directly on RGB pixel data.

### Grayscale Conversion

```text
Gray = 0.299R + 0.587G + 0.114B
```

The calculated intensity is assigned to all three color channels:

```text
R = Gray
G = Gray
B = Gray
```

### Brightness Adjustment

```text
R = R + Adjustment
G = G + Adjustment
B = B + Adjustment
```

Each resulting channel value is restricted to the valid range of `0` to `255`. The brightness input must be an integer between `-255` and `255`.

### Image Inversion

```text
R = 255 - R
G = 255 - G
B = 255 - B
```

### Horizontal Flip

```text
(x, y) ↔ (width - 1 - x, y)
```

Only half of each row is traversed because each swap processes two pixels.

### Vertical Flip

```text
(x, y) ↔ (x, height - 1 - y)
```

Only half of the image rows are traversed.

### 90-Degree Clockwise Rotation

Rotation creates a new image with exchanged dimensions:

```text
New width  = Original height
New height = Original width
```

Each source pixel is copied to its corresponding rotated coordinate.

### Crop

Crop input uses the following format:

```text
x y width height
```

Example:

```text
100 50 500 300
```

The crop rectangle is validated before memory is allocated. The selected pixel region is copied into a newly allocated image. The coordinate origin is the top-left corner.

### 3 × 3 Blur

For each output pixel, the program:

1. Finds the valid pixels in the surrounding `3 × 3` neighborhood.
2. Adds their red, green, and blue values separately.
3. Divides each sum by the number of valid neighbors.
4. Stores the result in a separate output buffer.

The separate output buffer prevents newly blurred pixels from affecting later calculations.

### Sharpening

The optional sharpening operation uses this convolution kernel:

```text
 0  -1   0
-1   5  -1
 0  -1   0
```

Each neighboring pixel value is multiplied by its corresponding kernel value. The final RGB values are restricted to the range of `0` to `255`. A separate output buffer prevents intermediate values from affecting later calculations.

---

## 🧱 Image Representation

Each pixel is represented by three unsigned 8-bit color components:

```c
typedef struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Pixel;
```

The complete image is represented by its dimensions and a dynamically allocated pixel array:

```c
typedef struct
{
    int width;
    int height;
    Pixel *data;
} Image;
```

A pixel at coordinate `(x, y)` is accessed using:

```c
image->data[y * image->width + x]
```

---

## ↩️ Multi-Level Undo

Before an image-processing operation is applied, the program creates a deep copy of the current full-resolution image.

The copies are stored in a dynamically allocated linked-list stack:

```text
Most recent previous state
            ↓
Earlier image state
            ↓
Original image state
```

When the user selects **Undo**:

1. The current image is released.
2. The most recent saved image is removed from the stack.
3. The saved image becomes the current image.
4. The display preview is rebuilt.

When no earlier state remains, the application displays an informational dialog instead of failing.

---

## 🧠 Dynamic Memory Management

Dynamic memory is allocated for:

- Image structures
- Full-resolution pixel arrays
- Rotated images
- Cropped images
- Blur output buffers
- Sharpen output buffers
- Responsive display previews
- Undo image snapshots
- Undo linked-list nodes

Memory is released when:

- A temporary processing buffer is no longer required
- A display preview is replaced
- The canvas changes size
- A different image is opened
- Undo history is cleared
- A new editing session begins
- The application exits

The project uses `free()` for dynamically allocated C memory and `IupDestroy()` for dynamically created IUP resources.

---

## 💬 Dialogs and Error Handling

The application includes dialogs for:

- Opening and saving BMP images
- Invalid or unsupported BMP files
- Attempts to edit without an open image
- Invalid brightness input
- Invalid crop boundaries
- Memory-allocation failures
- Large-image memory warnings
- Empty Undo history
- Clearing Undo history
- Unsaved changes
- Starting a new session
- Exiting the application
- Post-save actions
- Image information
- Help and application information

Invalid input is handled without intentionally terminating the application.

---

## 📁 Project Structure

```text
cse1101l-iup-bmp-image-editor/
├── src/
│   ├── main.c
│   ├── gui.c
│   ├── gui.h
│   ├── image.c
│   ├── image.h
│   ├── processing.c
│   ├── processing.h
│   ├── undo.c
│   └── undo.h
├── tests/
│   └── test_core.c
├── Makefile
└── README.md
```

### File Responsibilities

#### `src/main.c`

- Initializes the IUP library
- Calls `gui_create()` to create the main application window
- Displays the main dialog
- Starts the IUP event loop
- Calls `gui_cleanup()` before termination
- Destroys the main dialog and closes IUP

#### `src/gui.c`

- Creates and arranges the graphical interface
- Creates buttons, labels, layouts, dialogs, and the image canvas
- Registers and implements GUI callbacks
- Handles file dialog interactions
- Handles brightness and crop input dialogs
- Manages unsaved-change, exit, and post-save decisions
- Creates and refreshes responsive image previews
- Handles canvas drawing and window resizing
- Displays Image Information, Help, and About dialogs
- Coordinates current-image, preview, and Undo cleanup
- Displays the Large Image Warning when an Undo snapshot would require more than approximately 64 MB

#### `src/gui.h`

- Declares GUI creation and cleanup functions
- Exposes the GUI interface used by `main.c`

#### `src/image.c`

- Creates and releases image structures
- Allocates pixel arrays
- Creates deep image copies
- Loads 24-bit uncompressed BMP files
- Saves 24-bit uncompressed BMP files
- Handles BMP headers, BGR-to-RGB conversion, and row padding
- Reports file-format and memory errors

#### `src/image.h`

- Defines the `Pixel` and `Image` structures
- Declares image allocation, copying, deallocation, and BMP I/O functions

#### `src/processing.c`

- Implements grayscale conversion
- Implements brightness adjustment and channel clamping
- Implements inversion and flipping
- Implements rotation and cropping
- Implements separate-buffer `3 × 3` blur
- Implements convolution-based sharpening

#### `src/processing.h`

- Declares all image-processing functions used by the GUI callbacks

#### `src/undo.c`

- Implements the linked-list Undo stack
- Creates and stores deep image snapshots
- Restores the most recent image state
- Tracks Undo-state count and memory consumption
- Releases the complete Undo history

#### `src/undo.h`

- Declares Undo stack operations and Undo information functions

#### `tests/test_core.c`

- Tests image allocation and cleanup
- Tests BMP saving and loading
- Tests image-processing operations
- Tests Undo behavior

---

## 🖥️ Platform Compatibility

The project is configured and tested for **macOS on Apple Silicon**.

IUP is a multi-platform GUI toolkit, so the core C source code may be adapted for Windows and Linux. However, other platforms require compatible IUP libraries, compiler configurations, and platform-specific linker options.

The included Makefile is intended for the tested macOS environment.

---

## ✅ Project Compliance

The project demonstrates:

- Graphical user interface development using IUP
- IUP buttons, layouts, canvas, dialogs, and callbacks
- 24-bit uncompressed BMP input and output
- Custom pixel and image structures
- Dynamically allocated pixel arrays
- Pointer-based pixel access
- Manual implementation of all required algorithms
- Weighted grayscale conversion
- Brightness adjustment with channel clamping
- Image inversion
- Horizontal and vertical flipping
- 90-degree clockwise rotation
- Boundary-validated cropping
- Separate-buffer `3 × 3` blur
- Multi-level Undo
- Input validation
- Unsupported-file handling
- Dynamic memory cleanup
- Optional convolution-based sharpening

---

## 📄 Academic Use

This project was developed for academic and educational purposes as part of the **CSE1101L** course.

