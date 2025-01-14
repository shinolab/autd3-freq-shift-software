# AUTD3-freq-shift

This software is for changing the ultrasound frequency.

Version: 0.2.0

* The firmware codes are available at [here](https://github.com/shinolab/autd3-freq-shift-firmware).

## :books: [API document](https://shinolab.github.io/autd3-freq-shift-software/index.html)

## :memo: Versioning

The meanings of version number x.y.z are
* x: Firmware version
* y: Software version
* z: patch version

If the number of x changes, the firmware of FPGA or CPU must be upgraded.

## :fire: CAUTION

* Before using, be sure to write the latest firmwares in `dist/firmware`. For more information, please see [readme](/dist/firmware/Readme.md).

## :ballot_box_with_check: Requirements

* Install [Npcap](https://nmap.org/npcap/) with WinPcap API-compatible mode (recommended) or [WinPcap](https://www.winpcap.org/).

## :beginner: Example

See `client/examples`

If you are using Linux/macOS, you may need to run as root.

## :copyright: LICENSE

See [LICENSE](./LICENSE) and [NOTICE](./NOTICE).

# Author

Shun Suzuki, 2021
