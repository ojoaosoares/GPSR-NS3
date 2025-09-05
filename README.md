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

