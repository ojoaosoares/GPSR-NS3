# GPSR – Greedy Perimeter Stateless Routing for ns-3.29

This repository provides a **plugin for ns-3.29** that implements **GPSR (Greedy Perimeter Stateless Routing)** — a **stateless, greedy, geographic routing protocol**.

---

## What is GPSR?


More about **GPSR** can be read in this [paper](https://dl.acm.org/doi/abs/10.1145/345910.345953).


## Project Structure

This repository is meant to be cloned **inside the `src/` folder** of an unmodified [ns-3.29](https://www.nsnam.org) source tree.

## Requirements

- Linux or macOS
- `gcc` version **< 10** (e.g., 9.x)
- `python2.7` (available as `python2`)
- `make`, `git`, `wget`, `tar`
- Recommended compiler flags:
  
  ```bash
  export CXXFLAGS="-Wall -std=c++0x"
  ```

## Instalation

### Download ns3.29

```bash
wget https://www.nsnam.org/releases/ns-allinone-3.29.tar.bz2
tar -xjf ns-allinone-3.29.tar.bz2
cd ns-allinone-3.29/ns-3.29
```

### Add the GPSR module

Clone the location service repositoy, then the GPSR repository inside the src/ directory:

```bash
cd src
git clone https://github.com/ojoaosoares/Location-Service-NS3.git
mv Location-Service-NS3 location-service
git clone https://github.com/ojoaosoares/GPSR-NS3.git
mv GPSR-NS3 gpsr
```

## 3. Configure and Build

```bash
cd ..
./waf configure --enable-examples --enable-tests
./waf build
```

## 4. Run Examples

The GPSR protocol comes with ready-to-use examples located in src/gpsr/examples/.
To test them, you should copy the desired example into the scratch/ directory of your ns-3.29 installation and run it with waf.

## 5. GPSR Configuration

The GPSR module can be customized through the GpsrHelper class, which defines how the routing protocol behaves in terms of neighbor discovery, packet queuing, and planar graph construction during perimeter routing. Each simulation can adjust these parameters to tune GPSR behavior:



- helloInterval: Interval between periodic HELLO messages exchanged by nodes.

- maxQueueLen: Maximum number of packets stored in the queue while waiting for a valid route.

- maxQueueTime: Maximum time a packet can stay in the queue before being dropped.

- entryLifeTime: Lifetime of entries in the Position Table.

- graphType: Determines the planarization algorithm used in Perimeter Mode:

  - 0 → No planarization (use all links)

  - 1 → Gabriel Graph (GG)

  - 2 → Relative Neighborhood Graph (RNG)

```c

The GPSR helper constructor is shown below.:

GpsrHelper (Time helloInterval = Seconds (1),
              uint32_t maxQueueLen = 64,
              Time maxQueueTime = Seconds (30),
              Time entryLifeTime = Seconds (2),
              uint8_t graphType = 0);
```
   


## 6. Test Configuration

In gpsr/examples, you can find gpsr-test9.cc, which serves as a convenient template for any modifications you may want to make. In this example, the Random Waypoint mobility model is used, allowing nodes to move freely. You can easily replace it with any other mobility model if desired.

The initial positions of the nodes are randomized, but they can also be manually configured according to your needs. Additionally, a wide range of simulation parameters can be adjusted for your tests, including:

### General

- size: number of nodes

- time: simulation duration in seconds

- seed: random number generator seed

### Transmission Power

- txPowerStart: initial transmission power (dBm)

- txPowerEnd: final transmission power (dBm)

- txPowerLevels: number of transmission power levels

### Wi-Fi Settings

- rtsCts: RTS/CTS threshold (0 = always enabled, 2347 = disabled)

- phyMode: Wi-Fi physical mode (e.g., OfdmRate6Mbps)

### Application Layer

- packetSize: packet size in bytes

- maxPackets: total number of packets to send

- interval: inter-packet interval in seconds

### Mobility Model

- mapWidth / mapHeight: simulation area dimensions in meters

- speedMin / speedMax: node speed range in m/s

- pauseMin / pauseMax: minimum and maximum pause times in seconds