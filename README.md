# Lock'n'loop

Locks the image file using Linux Open File Descriptor (OFD) API and
sets up loop device with direct IO enabled, if supported by the
underlying file system.

The lock is compatible with QEMU locking so this can be used to mark
an images as mounted so you don't accidentally run QEMU on the same
image.

## Compiling

This compiles with both klibc and glibc. The glibc build doesn't
produce any compiler warnings but there are some warnings produced
with `klcc` but those come from the limitations of klibc.

With klibc you can build a small binary for initramfs use, for example
to lock image files over NFS.

To build normally (glibc):

```sh
gcc -Wall -o locknloop locknloop.c
```

To build static klibc version:

```sh
klcc -static -o locknloop locknloop.c
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
./locknloop file.img 60
```

Command outputs the freshly created loop device.

## Localization

In case you have very specific use case where you need to alter the
messages, you can provide them compile time.

```sh
klcc -static '-DMSG_WAIT="Ohjelmistopäivitykset ovat meneillään. Odotetaan %ld sekuntia."' '-DMSG_TIMEOUT="Ohjelmistopäivitykset ovat yhä kesken. Sammutetaan!"' -o locknloop locknloop.c
```
