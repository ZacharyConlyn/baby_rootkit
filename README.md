# Baby_rootkit

A loadable kernel module designed to hide things.

Baby_rootkit is a project for me to learn more about the Linux kernel, C programming, and malware development. **Please do not use this code for illegal, unethical, immoral, or uncool purposes.**

## Building

With Make and GCC installed, you can compile Baby_rootkit for your kernel with the command `make`. To delete the intermediate files, run `make clean`.

## Usage

### Instantiation

`# insmod baby_kit.ko` loads Baby_kit into the kernel.

When Baby_rootkit is loaded, the file `/dev/baby_rootkit` is created. The name of this file can be changed with the argument `filename="<some file name>"` when *insmod*ing 

## Command & Control

### Sending commands

Baby_rootkit reads commands that are written to its /dev/ file. Here is a list of commands supported currently.

### Currently implemented commands

#### hide

Remove Baby_kit from the list of loaded kernel modules visible with `lsmod`

#### unhide

Unhide Baby_kit from `lsmod`

### Status & error messages

*cat*ing the /dev/ file will display a status string relating to the last command.

## TODO

 * Add "clear" command to scrub buffers of previous commands/statuses
 * Add functions to hide/unhide files and directories
 * Hide the C2 files in `/dev/`, `/proc/devices`, and `/sys/class`
 * Add functions to hide/unhide processes
 * Add functions to hide/unhide network connections
 * Add hidden persistence
 * Add support/instructions for cross-compilation
