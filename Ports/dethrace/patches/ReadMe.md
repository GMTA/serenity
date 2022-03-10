# Patches for dethrace on SerenityOS

## `0001-Build-Teach-dethrace-how-to-find-LibGL.patch`

Build: Teach dethrace how to find LibGL


## `0002-Harness-Disable-Linux-specific-backtrace-and-signal-.patch`

Harness: Disable Linux-specific backtrace and signal logic

These are not supported and only get in the way of regular debugging
tools.

