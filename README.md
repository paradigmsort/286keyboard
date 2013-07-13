# 286Keyboard

A keyboard controller for the keyboard from the AGI 286/12 "portable computer",
using the Teensy++ 2.0.

Supports "logging mode":
  SysReq+D to dump, SysReq+R to reset

Supports up to 9 programmed sequences.
  SysReq+P+1..9 to start programming
  SysReq when done
  SysReq+1..9 to replay

Licensed under the MIT license (see LICENSE file).