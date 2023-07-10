# Lock'n'loop

Locks a file using Linux Open File Descriptor (OFD) API and
sets up a loop device with direct I/O enabled, if supported by the
underlying file system.

The lock is compatible with NFSv4 and with QEMU locking. This software
was written for a network booting setup where it is necessary to run
software updates with QEMU on the host but never at the same time when
that image is mounted over NFS. When using NFS, make sure NFSv4 is
used because OFD locking is not supported in earlier versions of NFS.
In my case, I'm ensuring it by using `nfsvers=4.2` mount option.

Of course, this software can be used to lock disk image files on a
local system, too.

The lock is automatically released when the loop device is
destructed. Because Linux loop devices support lazy device
destruction, after mounting the image you can safely run `losetup -d`
to mark the loop device as autoclear. The lock is not released until
the image is unmounted.

The cool thing is that the lock is passed to the kernel, so this
command doesn't need to hold an open file descriptor. So, unlike
`flock`, this is suitable for initramfs usage.

## Compiling

This compiles with glibc. Due to missing syscall support this doesn't
unfortunately compile with klibc.

To build (glibc):

```sh
gcc -Wall -Os -s -o locknloop locknloop.c
```

## Usage

To lock a file called `file.img`, failing immediately if lock can't be
obtained.

```sh
./locknloop file.img
```

To lock a file called `file.img`, waiting the lock for maximum of 1
minute. If lock is not available by then, fail.

```sh
./locknloop -t 60 file.img
```

Command outputs the freshly created loop device.

## Localization

In case you have a very specific use case where you need to alter the
messages, you can provide them at compile time.

```sh
gcc -Wall -Os -s -static '-DMSG_WAIT="Ohjelmistopäivitykset ovat meneillään. Odotetaan %ld sekuntia."' '-DMSG_TIMEOUT="Ohjelmistopäivitykset ovat yhä kesken. Sammutetaan!"' -o locknloop locknloop.c
```

## License

The contents of this repository are licensed under the GNU General Public
License version 3 or later. See file LICENSE for the license text.
