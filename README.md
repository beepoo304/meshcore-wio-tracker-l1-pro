# MeshCore for Wio Tracker L1 Pro â€” BLE Companion, 500 Contacts & Repeat Mode

Custom **MeshCore firmware for the Seeed Studio Wio Tracker L1 Pro**. This fork keeps the official **Companion BLE** experience for Android/iOS messaging, increases capacity to **500 contacts**, and allows **companion repeater mode on 869.618 MHz**.

Based on official [MeshCore companion-v1.17.1](https://github.com/meshcore-dev/MeshCore/tree/companion-v1.17.1). This is an independent community modification, not an official MeshCore or Seeed Studio release.

**[Download firmware UF2](https://github.com/beepoo304/meshcore-wio-tracker-l1-pro/releases/latest)** Â· [Upstream project](https://github.com/meshcore-dev/MeshCore)

## What changes on the Wio Tracker L1 Pro?

| Feature | This build |
|---|---|
| Firmware target | `WioTrackerL1_companion_radio_ble` |
| Phone connection | Bluetooth Low Energy (BLE) |
| Contact capacity | 500 instead of 350 |
| Companion Repeat | Adds 869.618 MHz to the upstream allowed list |
| Intended network settings | 869.618 MHz, BW 62.5 kHz, SF6, CR8 |
| Messaging and display | Original companion functionality |
| Existing settings | Preserved by an application-only update; no factory reset required |

Only two build configuration changes are made, both scoped to the Wio L1 BLE target. The upstream frequencies 433.000, 869.495 and 918.000 MHz remain in the repeat allowlist. The USB, repeater and other board targets are unchanged.

## Companion repeater with messaging

The official companion already implements packet forwarding, but v1.17.1 rejects enabling it on frequencies outside its allowlist. This build adds **869.618 MHz** to that list, which is also reported to the phone app. Connect the MeshCore app over BLE, keep your network's radio settings, then enable **Repeat Mode** in radio settings.

Forwarding is controlled by the app toggle; this build does not force it on or overwrite your saved frequency, bandwidth, SF or CR. It does not add the dedicated Repeater firmware's full administration interface. Forwarding runs on the device, while the phone remains the messaging interface. Use matching network settings and account for local radio requirements and channel airtime.

## Install as an update

1. Export contacts and settings from your app where supported before updating.
2. Download the **WioTrackerL1 companion BLE .uf2** from this repository's Releases.
3. Connect the Wio by USB and double-press Reset to enter its UF2 bootloader.
4. Copy the UF2 to the device's bootloader drive. It restarts automatically.
5. Reconnect the phone app over BLE and verify contacts and radio settings. Enable Repeat Mode if wanted.

Use an application UF2 update, without factory reset, erase firmware or a bootloader replacement. The contact storage format is unchanged. USB is the installation cable, not the companion transport in this build. A display of `350 contacts` is the current entry count, not the new maximum. The firmware version string remains `v1.17.1`; the release filename identifies the custom build.

## Validation and limitations

The build compiles successfully with 175,620 bytes static RAM (74.6% of PlatformIO's reported 235,520 bytes) and 432,952 bytes program flash. UF2 blocks were validated to address only `0x27000`â€“`0x90BFF`, below the data region beginning at `0xD4000`. The application update was copied to a Wio Tracker L1 Pro and the device re-enumerated over USB after restarting.

The earlier 500-contact-only build completed a BLE synchronization of 350 contacts. The additional repeat-frequency build still needs an end-to-end radio forwarding test and post-update contact confirmation. Successful compilation and USB restart do not establish repeater performance. No full-capacity 500-contact stress test has been performed.

1000 contacts do not fit the current RAM budget. The upstream capacity-reporting field also supports at most 510 contacts. This fork retains 500 without changing the app protocol.

## Build from source

Install Git and PlatformIO Core, check out this fork's `wio-l1-pro-companion` branch, then run:

```sh
python -m platformio run -e WioTrackerL1_companion_radio_ble
python bin/uf2conv/uf2conv.py -f 0xADA52840 -c .pio/build/WioTrackerL1_companion_radio_ble/firmware.hex -o WioTrackerL1-companion-500-repeat869618.uf2
```

The original MeshCore-patched Adafruit nRF52 framework pin is retained. Other upstream library version ranges may resolve differently over time. Private device backups, identities and contact databases are not included in this repository or release.

## Upstream documentation and credits

MeshCore and its contributors provide the original firmware. The original license and upstream documentation below are retained.

---

## About MeshCore

MeshCore is a lightweight, portable C++ library that enables multi-hop packet routing for embedded projects using LoRa and other packet radios. It is designed for developers who want to create resilient, decentralized communication networks that work without the internet.

## đź”Ť What is MeshCore?

MeshCore now supports a range of LoRa devices, allowing for easy flashing without the need to compile firmware manually. Users can flash a pre-built binary using tools like Adafruit ESPTool and interact with the network through a serial console.
MeshCore provides the ability to create wireless mesh networks, similar to Meshtastic and Reticulum but with a focus on lightweight multi-hop packet routing for embedded projects. Unlike Meshtastic, which is tailored for casual LoRa communication, or Reticulum, which offers advanced networking, MeshCore balances simplicity with scalability, making it ideal for custom embedded solutions, where devices (nodes) can communicate over long distances by relaying messages through intermediate nodes. This is especially useful in off-grid, emergency, or tactical situations where traditional communication infrastructure is unavailable.

## âšˇ Key Features

* Multi-Hop Packet Routing
  * Devices can forward messages across multiple nodes, extending range beyond a single radio's reach.
  * Supports up to a configurable number of hops to balance network efficiency and prevent excessive traffic.
  * Nodes use fixed roles where "Companion" nodes are not repeating messages at all to prevent adverse routing paths from being used.
* Supports LoRa Radios â€“ Works with Heltec, RAK Wireless, and other LoRa-based hardware.
* Decentralized & Resilient â€“ No central server or internet required; the network is self-healing.
* Low Power Consumption â€“ Ideal for battery-powered or solar-powered devices.
* Simple to Deploy â€“ Pre-built example applications make it easy to get started.

## đźŽŻ What Can You Use MeshCore For?

* Off-Grid Communication: Stay connected even in remote areas.
* Emergency Response & Disaster Recovery: Set up instant networks where infrastructure is down.
* Outdoor Activities: Hiking, camping, and adventure racing communication.
* Tactical & Security Applications: Military, law enforcement, and private security use cases.
* IoT & Sensor Networks: Collect data from remote sensors and relay it back to a central location.

## đźš€ How to Get Started

- Watch the [MeshCore QuickStart Playlist](https://www.youtube.com/watch?v=iaFltojJrAc&list=PLshzThxhw4O4WU_iZo3NmNZOv6KMrUuF9) by The Comms Channel
- Watch the [MeshCore Technical Presentation](https://www.youtube.com/watch?v=OwmkVkZQTf4) by Liam Cottle.
- Read through our [Frequently Asked Questions](./docs/faq.md) and [Documentation](https://docs.meshcore.io).
- Flash the MeshCore firmware on a supported device.
- Connect with a supported client.

For developers:

- Install [PlatformIO](https://docs.platformio.org) in [Visual Studio Code](https://code.visualstudio.com).
- Clone and open the MeshCore repository in Visual Studio Code.
- See the example applications you can modify and run:
  - [Companion Radio](./examples/companion_radio) - For use with an external chat app, over BLE, USB or Wi-Fi.
  - [KISS Modem](./examples/kiss_modem) - Serial KISS protocol bridge for host applications. ([protocol docs](./docs/kiss_modem_protocol.md))
  - [Simple Repeater](./examples/simple_repeater) - Extends network coverage by relaying messages.
  - [Simple Room Server](./examples/simple_room_server) - A simple BBS server for shared Posts.
  - [Simple Secure Chat](./examples/simple_secure_chat) - Secure terminal based text communication between devices.
  - [Simple Sensor](./examples/simple_sensor) - Remote sensor node with telemetry and alerting.

The Simple Secure Chat example can be interacted with through the Serial Monitor in Visual Studio Code, or with a Serial USB Terminal on Android.

## âšˇď¸Ź MeshCore Flasher

We have prebuilt firmware ready to flash on supported devices.

- Launch https://meshcore.io/flasher
- Select a supported device
- Flash one of the firmware types:
  - Companion, Repeater or Room Server
- Once flashing is complete, you can connect with one of the MeshCore clients below.

## đź“± MeshCore Clients

**Companion Firmware**

The companion firmware can be connected to via BLE, USB or Wi-Fi depending on the firmware type you flashed.

- Web: https://app.meshcore.nz
- Android: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
- iOS: https://apps.apple.com/us/app/meshcore/id6742354151?platform=iphone
- NodeJS: https://github.com/liamcottle/meshcore.js
- Python: https://github.com/fdlamotte/meshcore-cli

**Repeater and Room Server Firmware**

The repeater and room server firmware can be set up via USB in the web config tool.

- https://config.meshcore.io

They can also be managed via LoRa in the mobile app by using the Remote Management feature.

## đź›  Hardware Compatibility

MeshCore is designed for devices listed in the [MeshCore Flasher](https://meshcore.io/flasher)

## đź“ś License

MeshCore is open-source software released under the MIT License. You are free to use, modify, and distribute it for personal and commercial projects.

## Contributing

Please submit PR's using 'dev' as the base branch!
For minor changes just submit your PR and we'll try to review it, but for anything more 'impactful' please open an Issue first and start a discussion. It is better to sound out what it is you want to achieve first, and try to come to a consensus on what the best approach is, especially when it impacts the structure or architecture of this codebase.

Here are some general principles you should try to adhere to:
* Keep it simple. Please, don't think like a high-level lang programmer. Think embedded, and keep code concise, without any unnecessary layers.
* No dynamic memory allocation, except during setup/begin functions.
* Use the same brace and indenting style that's in the core source modules. (A .clang-format is probably going to be added soon, but please do NOT retroactively re-format existing code. This just creates unnecessary diffs that make finding problems harder)

Help us prioritize! Please react with thumbs-up to issues/PRs you care about most. We look at reaction counts when planning work.

### Running unit tests

To run unit tests, run the following command:

```bash
pio test --environment native --verbose
```

## Road-Map / To-Do

There are a number of fairly major features in the pipeline, with no particular time-frames attached yet. In very rough chronological order:
- [X] Companion radio: UI redesign
- [X] Repeater + Room Server: add ACL's (like Sensor Node has)
- [X] Standardise Bridge mode for repeaters
- [ ] Repeater/Bridge: Standardise the Transport Codes for zoning/filtering
- [X] Core + Repeater: enhanced zero-hop neighbour discovery
- [ ] Core: round-trip manual path support
- [ ] Companion + Apps: support for multiple sub-meshes (and 'off-grid' client repeat mode)
- [ ] Core + Apps: support for LZW message compression
- [ ] Core: dynamic CR (Coding Rate) for weak vs strong hops
- [ ] Core: new framework for hosting multiple virtual nodes on one physical device
- [ ] V2 protocol spec: discussion and consensus around V2 packet protocol, including path hashes, new encryption specs, etc

## đź“ž Get Support

- Report bugs and request features on the [GitHub Issues](https://github.com/ripplebiz/MeshCore/issues) page.
- Find additional guides and components on [my site](https://buymeacoffee.com/ripplebiz).
- Join [MeshCore Discord](https://meshcore.gg) to chat with the developers and get help from the community.
