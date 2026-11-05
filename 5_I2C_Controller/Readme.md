# Custom I2C EEPROM Linux Driver — Raspberry Pi 4

A learning project that implements a custom Linux I2C character driver for an I2C EEPROM module connected to a Raspberry Pi 4.

The project deliberately uses a **custom EEPROM driver** rather than the in-kernel `at24` driver. The EEPROM is described through a Device Tree Overlay, the I2C core creates the I2C client, and the custom driver binds to that client through its Device Tree `compatible` string.

> **Hardware assumption:** The examples below use a 24C256-compatible EEPROM. Confirm the exact IC marking and datasheet for your module before relying on its capacity, page size, write-cycle timing, voltage, or pin behavior. The module's IC marking was not clearly readable in the supplied photograph.

## Project status

| Feature | Status |
|---|---|
| Custom I2C client driver and `probe()` / `remove()` | Implemented and working |
| Character device interface (`/dev/my_i2c-80`) | Implemented and working |
| EEPROM byte reads | Working |
| EEPROM page-wise writes | Implemented and working |
| ACK polling after page writes | Implemented; error handling addressed |
| Device Tree Overlay and OF matching | Implemented and working |
| Duplicate I2C client registration removed | Fixed |
| File-position handling (`*ppos`) | Fixed |
| `fsync()` callback | Discussed / proposed; verify in source and test before marking complete |
| `ioctl()` (`MY_EEPROM_GET_INFO`) | Interface and example handler designed; verify in source and test before marking complete |
| Multi-device support | Future work |
| DMA changes | Not implemented; see [DMA design note](#dma-design-note) |

## 1. Hardware and assumed EEPROM characteristics

The module has the usual I2C and configuration pins:

| Module pin | Purpose | Project handling |
|---|---|---|
| VCC | EEPROM supply | Connect according to the IC/module datasheet |
| GND | Ground | Common ground with the Raspberry Pi |
| SDA | I2C data | Connected to the Pi's I2C SDA pin |
| SCL | I2C clock | Connected to the Pi's I2C SCL pin |
| WP | Write protect | Not controlled by this driver; left at the module's hardware default |
| A0, A1, A2 | I2C address selection | Not controlled by this driver; left at the module's hardware defaults |

For the standard Raspberry Pi 4 `i2c1` connection:

| I2C signal | BCM GPIO | 40-pin header pin |
|---|---:|---:|
| SDA1 | GPIO2 | Pin 3 |
| SCL1 | GPIO3 | Pin 5 |

The I2C controller and pinctrl configuration own SDA/SCL. The EEPROM child node does **not** need `sda-gpios` or `scl-gpios` properties. The EEPROM driver uses the I2C API (`i2c_transfer()`, `i2c_master_send()`) and does not bit-bang those pins.

For a 24C256-compatible part, the commonly used parameters are:

| Parameter | Assumed value |
|---|---:|
| Capacity | 256 Kbit = 32 KiB = 32768 bytes |
| Valid memory offsets | `0x0000`–`0x7FFF` |
| Page size | 64 bytes |
| Internal memory-address width | 2 bytes (16 bits) |
| Typical 7-bit I2C address | `0x50` when A0/A1/A2 are LOW |

Use the actual datasheet to confirm these values. Do not assume that every 24Cxx part has the same capacity or page size.

## 2. High-level software architecture

```text
                  Raspberry Pi 4
                       |
             Base Device Tree + Overlay
                       |
           I2C controller / i2c1 / pinctrl
                       |
            I2C client at address 0x50
                       |
              Linux I2C driver matching
          compatible = "myproject,my-eeprom"
                       |
                 my_i2c_probe()
                       |
        Character device registration (cdev)
                       |
                 /dev/my_i2c-80
                       |
            userspace read() / write()
                       |
            my_i2c_read()/my_i2c_write()
                       |
         copy_to_user()/copy_from_user()
                       |
       at24c256_hw_read()/at24c256_hw_write()
                       |
       Linux I2C core and Pi I2C controller
                       |
                    EEPROM
```

The implementation is split into two logical layers:

1. **Character-device layer:** handles userspace buffers, file positions, locking, and the `/dev` interface.
2. **EEPROM hardware layer:** handles the EEPROM's internal memory address, I2C messages, page boundaries, and write-cycle completion.

Keeping these responsibilities separate makes the code easier to reason about and test.

## 3. Device Tree Overlay

The overlay describes the EEPROM as a child of the Raspberry Pi ARM I2C controller. It does not describe SDA/SCL as GPIOs because the controller's base Device Tree and pinctrl setup already describe the bus pins.

Example `my-eeprom-overlay.dts`:

```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "brcm,bcm2711";

    fragment@0 {
        target = <&i2c_arm>;

        __overlay__ {
            status = "okay";

            eeprom@50 {
                compatible = "myproject,my-eeprom";
                reg = <0x50>;
            };
        };
    };
};
```

Important properties:

- `&i2c_arm` references the existing Raspberry Pi ARM I2C controller (the standard ARM I2C bus is I2C1 on Pi 4).
- `eeprom@50` is the child node name; `50` corresponds to the address in `reg`.
- `reg = <0x50>` is the EEPROM's **I2C slave address**, not its internal memory offset.
- `compatible` must exactly match an entry in the driver's `of_match_table`.

Compile and install:

```bash
sudo apt install device-tree-compiler

dtc -@ -I dts -O dtb \
    -o my-eeprom.dtbo \
    my-eeprom-overlay.dts

sudo cp my-eeprom.dtbo /boot/firmware/overlays/
```

Add to `/boot/firmware/config.txt`:

```ini
dtparam=i2c_arm=on
dtoverlay=my-eeprom
```

Then reboot:

```bash
sudo reboot
```

Verify the I2C client was created:

```bash
ls /sys/bus/i2c/devices/
ls -l /sys/bus/i2c/devices/1-0050/of_node
```

The exact bus number can differ if the board configuration changes. On the standard Pi 4 setup, the client is expected at `1-0050`.

## 4. Driver matching and registration

The driver needs an OF match table whose compatible string matches the overlay:

```c
static const struct of_device_id my_i2c_of_match[] = {
    { .compatible = "myproject,my-eeprom" },
    { }
};
MODULE_DEVICE_TABLE(of, my_i2c_of_match);
```

Register it in `struct i2c_driver`:

```c
static struct i2c_driver my_i2c_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = my_i2c_of_match,
    },
    .probe = my_i2c_probe,
    .remove = my_i2c_remove,
    .id_table = my_i2c_id, /* Optional if legacy I2C ID matching is also wanted */
};
```

The module initialization should create the character-device class and register the I2C driver. The I2C core then matches the already-created Device Tree client and calls `my_i2c_probe()`.

Example lifecycle:

```c
static int __init my_driver_init(void)
{
    int ret;

    my_i2c_class = class_create(CLASS_NAME);
    if (IS_ERR(my_i2c_class))
        return PTR_ERR(my_i2c_class);

    ret = i2c_add_driver(&my_i2c_driver);
    if (ret) {
        class_destroy(my_i2c_class);
        return ret;
    }

    return 0;
}

static void __exit my_driver_exit(void)
{
    i2c_del_driver(&my_i2c_driver);
    class_destroy(my_i2c_class);
}
```

Keep the class creation/destruction appropriate to the kernel version in use. The example above uses the one-argument `class_create(CLASS_NAME)` form used by current kernels.

### Do not create a second client manually

When Device Tree creates the client, do **not** also call `i2c_new_client_device()` for the same bus/address. Use one device-instantiation mechanism. Device Tree owns the client's existence; the module registers and unregisters the driver.

## 5. Character-device design

The I2C client's `probe()` function prepares the per-device state and registers the character device. In this project it creates a node similar to:

```text
/dev/my_i2c-80
```

The device structure typically needs to hold the I2C client, mutex, cdev, allocated device number, and the kernel buffers used by the read/write paths. The exact buffer representation (embedded array versus allocated pointer) must be consistent with the checks and allocations in the code.

Conceptual responsibilities:

| Callback | Responsibility |
|---|---|
| `open()` | Put the device structure in `file->private_data` |
| `read()` | Read from EEPROM at `*ppos`, copy bytes to userspace, advance `*ppos` |
| `write()` | Copy bytes from userspace, write at `*ppos`, advance `*ppos` |
| `llseek()` | Restrict seeks to the EEPROM's valid range |
| `fsync()` | Optional readiness/synchronization operation; see note below |
| `unlocked_ioctl()` | Handle device-specific control/query commands |
| `release()` | Per-open cleanup if needed |

Use a mutex when shared kernel buffers or device state are accessed by multiple operations. Do not hold a mutex across atomic/interrupt contexts; these file operations run in process context and can sleep.

## 6. Read path logic

The read path follows this sequence:

1. `my_i2c_read()` receives a userspace pointer, byte count, and file position (`loff_t *ppos`).
2. Validate the device pointer, position, length, and EEPROM bounds.
3. Limit the transfer to the EEPROM capacity and kernel buffer capacity.
4. Acquire the device mutex.
5. Call the hardware helper with the **numeric offset value**.
6. The hardware helper sends the two-byte EEPROM internal memory address, followed by an I2C read message.
7. Copy the received kernel buffer to userspace with `copy_to_user()`.
8. Advance `*ppos` only after the data was successfully copied to userspace.
9. Release the mutex.

The I2C read is a combined transaction:

```text
START
  -> EEPROM I2C address + WRITE
  -> internal memory address MSB
  -> internal memory address LSB
REPEATED START
  -> EEPROM I2C address + READ
  -> data bytes
STOP
```

`i2c_transfer(adapter, msgs, 2)` returns a **message count**, not a byte count. Success requires `ret == 2`. A negative value is an error; a non-negative value other than 2 is an incomplete transfer and must not be reported as a successful full read.

## 7. Write path logic

The write path has two layers.

### 7.1 Character-device `write()` layer

1. Validate the device, `ppos`, and EEPROM bounds.
2. Limit the requested size to the kernel buffer capacity and remaining EEPROM capacity.
3. Acquire the mutex.
4. Use `copy_from_user()` to copy the application data into a kernel buffer.
5. Call `at24c256_hw_write()` with the data and numeric starting offset.
6. Advance `*ppos` by the positive number of bytes returned by the hardware helper.
7. Release the mutex.

The character-device layer owns the file position. The hardware helper must not increment the caller's `*ppos`.

### 7.2 Hardware page-write layer

For a 24C256-compatible device, each page is 64 bytes. A page-write transaction must not cross a page boundary because the EEPROM's internal page address counter wraps within the current page.

The driver calculates the safe chunk length as:

```c
page_offset = cur_off % AT24C256_PAGE_SIZE;
space_in_page = AT24C256_PAGE_SIZE - page_offset;
chunk_len = min(bytes_left, space_in_page);
```

Then it prepares the I2C payload:

```text
+------------------+------------------+------------------+
| Memory addr MSB  | Memory addr LSB  | Payload bytes    |
+------------------+------------------+------------------+
| 1 byte           | 1 byte           | chunk_len bytes  |
+------------------+------------------+------------------+
```

For example, writing `HELLO` at offset `0x003E`:

- Page 0 covers `0x0000`–`0x003F`; only two bytes remain.
- Transaction 1 writes `H`, `E` at `0x003E`–`0x003F`.
- Transaction 2 writes `L`, `L`, `O` at `0x0040`–`0x0042`.

The I2C slave address (for example `0x50`) is separate from the EEPROM's internal memory address (for example `0x003E`). The I2C core uses `client->addr` for the bus address; the driver sends the two-byte memory offset as payload.

## 8. EEPROM internal write cycle and ACK polling

A successful I2C transfer does not by itself mean the EEPROM's non-volatile programming cycle has finished.

Typical page-write sequence:

1. The controller sends START, the device address, the internal memory address, and page data.
2. The controller sends STOP.
3. The EEPROM begins its internal programming cycle.
4. While programming, the EEPROM may NACK its I2C address.
5. The driver performs ACK polling until the EEPROM ACKs or the timeout expires.
6. Only after successful readiness polling does the driver count the page as confirmed complete.

The ACK-poll helper should:

- Have a bounded deadline.
- Retry only expected address-NACK errors for the target I2C adapter (commonly `-EREMOTEIO` or `-ENXIO`; confirm the adapter's actual behavior).
- Return immediately for unrelated bus/controller errors.
- Treat an incomplete non-negative message count as an error.
- Return `-ETIMEDOUT` if the deadline expires.
- Have its return value checked by the page-write function.

Example policy:

```c
if (ret == 1)
    return 0;                 /* Address acknowledged */

if (ret >= 0)
    return -EIO;              /* Incomplete message transfer */

if (ret != -EREMOTEIO && ret != -ENXIO)
    return ret;               /* Not an expected busy/NACK result */

/* Expected NACK: wait briefly and retry until deadline. */
```

A 10 ms polling deadline was used in the project discussion as a margin for a device with a shorter specified write-cycle time. Verify the actual EEPROM datasheet. Do not copy timing values blindly across EEPROM models.

## 9. Partial writes and interrupted writes

The page-write helper tracks:

- `bytes_left`: bytes not yet attempted/completed.
- `written_total`: bytes whose page programming was confirmed.
- `cur_off`: current EEPROM memory address used by the page-splitting loop.

Update these counters only after the I2C write transaction succeeds **and** ACK polling confirms that the EEPROM is ready:

```c
bytes_left -= chunk_len;
written_total += chunk_len;
cur_off += chunk_len;
```

If a later page fails after earlier pages were confirmed, return the positive `written_total`. The character-device layer advances `*ppos` by that positive amount.

If no bytes were confirmed, return the negative error. Before beginning another page, a pending signal may be checked with `signal_pending(current)`. If a signal is pending before any progress, return `-ERESTARTSYS`; if earlier pages are confirmed, return the positive byte count so userspace can account for partial progress.

**Important uncertainty:** An I2C write may have been accepted by the EEPROM even if a later transfer error or ACK-poll timeout prevents the driver from confirming completion. Do not claim that such a page definitely was not programmed. A userspace retry after an ambiguous timeout can repeat data; design applications accordingly, and do not blindly retry non-idempotent operations.

## 10. File-position handling: bug and fix

A major bug encountered during testing caused the read command to return the first byte repeatedly (`68 68 68 68 68`, or `hhhhh`) instead of `68 65 6c 6c 6f` (`hello`).

Cause:

- The character driver's `*ppos += ret` update had been commented out.
- Every read therefore used EEPROM offset zero.
- The hardware helper read the same first byte repeatedly.

The design rule used to fix it:

- The character-device layer owns and updates the file position.
- Hardware helpers receive an offset **by value** (`loff_t off`) and perform I2C at that address.
- The hardware helpers do not update the file position.

For a successful read or write:

```c
*ppos += ret;
```

For reads, advance the position only after `copy_to_user()` succeeds. For writes, advance it by the positive number of confirmed bytes returned by the hardware helper.

Do not both increment the position in the hardware helper and increment it again in the character-device wrapper.

## 11. Bugs encountered and how they were fixed

### 11.1 Garbage when using `cat` on the device

**Symptom:** `cat /dev/my_i2c-80` displayed binary-looking characters.

**Explanation:** The EEPROM is raw byte-addressable storage, not a text file. `cat` reads until EOF and can display old or non-printable bytes from the rest of the EEPROM.

**Better test:** Read only the known bytes and display them as hex:

```bash
sudo dd if=/dev/my_i2c-80 bs=1 count=5 status=none | hexdump -C
```

### 11.2 Incorrect cast of EEPROM offset

**Bug:**

```c
u16 mem_addr = (u16)off;
```

When `off` is a `loff_t *`, this casts the pointer value, not the offset stored at that address.

**Correct if using a pointer parameter:**

```c
u16 mem_addr = (u16)(*off);
```

**Preferred project interface:** Pass the numeric offset by value to the hardware helper:

```c
static ssize_t at24c256_hw_read(..., loff_t off);
```

Then use:

```c
u16 mem_addr = (u16)off;
```

### 11.3 Misinterpreting `i2c_transfer()` return value

**Bug:** Treating a non-negative return from `i2c_transfer()` as success without checking the message count.

**Explanation:** `i2c_transfer()` returns the number of messages completed, not the number of bytes.

**Fix for a two-message combined read:**

```c
ret = i2c_transfer(client->adapter, msgs, 2);
if (ret < 0)
    return ret;
if (ret != 2)
    return -EIO;
```

### 11.4 Repeated first byte (`hhhhh` instead of `hello`)

**Symptom:** A five-byte read returned the first byte five times.

**Cause:** `*ppos += ret` was commented out in the character-device read callback. Each one-byte read started at EEPROM offset zero.

**Fix:** Update `*ppos` in `my_i2c_read()` after `copy_to_user()` succeeds. Keep offset management out of the hardware helper.

### 11.5 Page-write crossing a 64-byte boundary

**Risk:** Sending data past the end of a page in one page-write command can wrap the EEPROM's internal page address counter and overwrite bytes at the beginning of that same page.

**Fix:** Split writes at page boundaries using `page_offset`, `space_in_page`, and `chunk_len` calculations (see [Write path logic](#7-write-path-logic)).

### 11.6 Ignored ACK-poll result

**Bug:** The page-write code called `at24c256_ack_poll()` but ignored its return value and continued as if the page had completed.

**Fix:** Check the return value. Stop on timeout or unrelated I2C error. Count a page as confirmed only after ACK polling succeeds.

### 11.7 Retrying every ACK-poll error

**Bug:** The original ACK-poll loop retried every unsuccessful `i2c_transfer()` result until timeout.

**Fix:** Retry only the expected address-NACK result(s) for the adapter, commonly `-EREMOTEIO` or `-ENXIO`. Return other errors immediately. Confirm the exact error mapping for the controller driver in use.

### 11.8 Partial I2C write not detected

**Bug:** Checking only `ret < 0` after `i2c_master_send()`.

**Fix:** Check that the exact requested number of bytes were transferred:

```c
ret = i2c_master_send(client, write_buf, chunk_len + 2);
if (ret < 0)
    return ret;
if (ret != chunk_len + 2)
    return -EIO;
```

### 11.9 NULL check after dereferencing the write buffer

**Bug:** The code wrote to `dev->write_buf[0]` and `[1]` before validating `dev->write_buf`.

**Fix:** Validate `dev`, `dev->client`, the write buffer, and the input buffer before dereferencing them. If the buffer is an embedded array rather than a pointer, do not test the array itself for NULL; validate the containing structure and its allocation instead.

### 11.10 EEPROM capacity overrun

**Risk:** A request may extend beyond the final EEPROM address.

**Fix:** Validate negative offsets and cap the transfer to the remaining capacity before performing I2C operations:

```c
if (off < 0)
    return -EINVAL;
if (off >= EEPROM_SIZE)
    return -ENOSPC; /* write */

count = min_t(size_t, count, EEPROM_SIZE - off);
```

For reads, reaching the capacity normally returns EOF (`0`).

### 11.11 Duplicate I2C client registration (`-16`, `-EBUSY`)

**Observed log pattern:**

```text
my_i2c_driver 1-0050: I2C driver probed successfully for addr 0x50
i2c i2c-1: Failed to register i2c client my_i2c_dev at 0x50 (-16)
Failed to instantiate I2C client at 0x50
my_i2c_driver 1-0050: I2C device removed
```

**Cause:** Device Tree had already created client `1-0050`, but the module also called `i2c_new_client_device()` for address `0x50` on adapter 1.

**Fix:** Remove `i2c_get_adapter()`, `i2c_new_client_device()`, and the matching manual `i2c_unregister_device()` from this Device Tree-based design. Register only the driver with `i2c_add_driver()`. On module exit, call `i2c_del_driver()`; the Device Tree-created client remains owned by the I2C/Device Tree infrastructure.

### 11.12 Confusion about SDA/SCL and GPIO configuration

**Question:** How does the Device Tree know which Pi pins connect to EEPROM SDA and SCL?

**Explanation:** The parent I2C controller's base Device Tree and pinctrl configuration describe the controller's pins. On the standard Pi 4 I2C1 mapping, SDA1 is GPIO2 / header pin 3 and SCL1 is GPIO3 / header pin 5. The EEPROM node is simply a child of the I2C controller.

**Fix:** Do not add `sda-gpios` or `scl-gpios` to this EEPROM child. The controller driver and pinctrl manage those signals. WP/A0/A1/A2 need GPIO descriptors only if the design actually wires them to controllable GPIOs; in this project they are left at module hardware defaults.

### 11.13 Out-of-tree module taint message

**Observed message:**

```text
at24c256_driver: loading out-of-tree module taints kernel.
```

**Explanation:** This is the kernel reporting that an externally built module has been loaded. It is not the cause of the I2C `-EBUSY` issue. The duplicate client registration caused `-EBUSY`.

## 12. `fsync()` considerations

The EEPROM write callback currently sends the bytes to the device and waits for ACK polling before reporting a page as confirmed. It does not use a Linux page cache or defer writes into a software queue.

Therefore, a custom `fsync()` callback that only performs another ACK poll is an **additional readiness check**, not a flush of buffered application data. It can be useful as an API exercise or as a final readiness check, but it should not be described as flushing a write-back cache that the driver does not have.

If the driver later adds queued/asynchronous writes, `fsync()` would need to wait for and report completion of all pending writes covered by the synchronization operation.

## 13. `ioctl()` interface (planned / example)

The first proposed ioctl is `MY_EEPROM_GET_INFO`, which returns static device parameters to userspace. This is a useful first ioctl because it does not modify EEPROM contents.

Shared header example `my_eeprom_ioctl.h`:

```c
#ifndef MY_EEPROM_IOCTL_H
#define MY_EEPROM_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MY_EEPROM_IOC_MAGIC 'E'

struct my_eeprom_info {
    __u32 size_bytes;
    __u32 page_size;
    __u16 i2c_addr;
    __u8  mem_addr_bytes;
    __u8  reserved;
};

#define MY_EEPROM_GET_INFO \
    _IOR(MY_EEPROM_IOC_MAGIC, 0x00, struct my_eeprom_info)

#endif
```

The driver handles the command through `.unlocked_ioctl` and returns the structure with `copy_to_user()`. A native 64-bit userspace test can call:

```c
struct my_eeprom_info info = {0};
if (ioctl(fd, MY_EEPROM_GET_INFO, &info) < 0)
    perror("ioctl");
```

Keep the ioctl command definitions in a shared header so userspace and kernel use exactly the same command encoding and structure layout. Add `.compat_ioctl = compat_ptr_ioctl` only when the interface is compatible with that simple pointer conversion and 32-bit userspace support is required. This ioctl feature was designed in the project discussion; confirm it has been added and tested in the source before treating it as complete.

## 14. DMA design note

`copy_from_user()` and `copy_to_user()` cannot simply be replaced with DMA calls.

They solve a different problem:

- `copy_from_user()` safely copies from a userspace virtual address into kernel memory.
- `copy_to_user()` safely copies from kernel memory into a userspace virtual address.
- DMA transfers data between memory and a hardware device using DMA-capable memory and the DMA mapping API.

The EEPROM client driver uses the Linux I2C API. The Raspberry Pi I2C controller driver is responsible for deciding how the controller moves data (for example, programmed I/O or DMA, depending on the controller driver, transfer size, and implementation). A client driver should not independently start DMA against the I2C controller's registers or pass a userspace pointer as a DMA address.

The safe normal architecture remains:

```text
userspace buffer
    -> copy_from_user()
    -> kernel buffer
    -> I2C core / adapter driver
    -> controller hardware (and any DMA used internally by the adapter)
```

For reads, the reverse path uses `copy_to_user()`. Keep these user-copy operations unless you are deliberately designing a different, fully specified userspace/DMA buffer interface. Direct userspace DMA would require a much more involved design and is not an appropriate shortcut for this EEPROM character driver.

## 15. Build, load, and test

The exact module build commands depend on the project's Makefile and kernel headers. A typical out-of-tree module workflow is:

```bash
make clean
make
sudo rmmod at24c256_driver   # only if currently loaded
sudo insmod at24c256_driver.ko
```

Check binding and logs:

```bash
ls -l /sys/bus/i2c/devices/1-0050/
readlink /sys/bus/i2c/devices/1-0050/driver
dmesg | tail -50
ls -l /dev/my_i2c-80
```

Write and read a short known payload:

```bash
printf 'hello' | sudo tee /dev/my_i2c-80 >/dev/null
sudo dd if=/dev/my_i2c-80 bs=1 count=5 status=none | hexdump -C
```

Expected data bytes:

```text
68 65 6c 6c 6f
```

`printf` is used instead of `echo` to avoid an additional newline byte. `dd` limits the read to five bytes so the test does not dump the entire binary EEPROM contents.

Other useful checks:

```bash
sudo i2cdetect -y 1
ls /sys/bus/i2c/devices/
dmesg | grep -iE 'eeprom|i2c|ack|timeout'
```

Use `i2cdetect` carefully and avoid running other tools that access the same EEPROM concurrently with your driver. The I2C address appearing in `i2cdetect` is not, by itself, proof that your custom driver is correctly bound.

## 16. Suggested source layout

A possible source layout for keeping the project organized:

```text
my-eeprom-driver/
├── README.md
├── Makefile
├── my_eeprom_ioctl.h
├── at24c256.h
├── my_i2c_main.c          # i2c_driver, probe/remove, module init/exit
├── my_i2c_fops.c          # open/read/write/llseek/fsync/ioctl/release
├── at24c256_hw.c          # I2C read/write, page split, ACK polling
├── my-eeprom-overlay.dts
└── userspace/
    ├── eeprom_ioctl_test.c
    └── eeprom_fsync_test.c
```

This is a suggested organization; filenames can differ in the actual project.

## 17. Next improvements

1. Finish and test `MY_EEPROM_GET_INFO` ioctl.
2. Add a status ioctl that reports readiness using ACK polling, with clear documentation that a poll may itself fail due to bus errors.
3. Add robust testing for partial writes and signal interruption using a userspace test program.
4. Review `remove()` and all error paths for complete cleanup of the cdev, device node, and allocated device number.
5. Consider multiple EEPROM devices by keeping all state per `i2c_client` and avoiding single-device global client pointers.
6. Add a binding document/schema for the custom compatible string if the driver is intended for broader reuse or upstream integration.

---

## Key design rules to remember

- Let Device Tree instantiate the I2C client; do not manually instantiate the same client again.
- Let the I2C controller and pinctrl manage SDA/SCL; the EEPROM client driver should use the I2C API.
- Keep I2C slave address separate from the EEPROM's internal memory offset.
- Split page writes so no I2C page-write transaction crosses a physical EEPROM page boundary.
- Wait for the EEPROM's internal write cycle using ACK polling, and handle its return value.
- Treat `i2c_transfer()` return values as message counts and `i2c_master_send()` return values as byte counts.
- Let the character-device layer own `*ppos`; hardware helpers receive offsets by value.
- Use `copy_from_user()` / `copy_to_user()` for safe userspace-kernel transfers.
- Count a page as confirmed only after the transfer succeeds and EEPROM readiness is confirmed.
- Treat a timeout as an uncertain completion state, not proof that the EEPROM definitely did not program the page.
