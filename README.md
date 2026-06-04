# loclogpp
Simple GPSD based location logger written in C++

## Roadmap

- [x] Basic point recording
- [x] Export database as GPX
- [ ] Sending points to Owntracks-like HTTP endpoint
- [x] Minimum displacement (only record point if X meters away from previous)
  - [x] Configurable option
- [x] Minimum accuracy (only record point if accuracy below threshold)
  - [x] Configurable option
- [ ] Stationary state detection
  - [ ] Reduce point frequency when stationary (configurable "heartbeat frequency"?)
- [ ] Dynamic minimum displacement detection (lower frequency on higher speed)
- [ ] Periodically log (at least on debug level) current fix infos (mode, sattelites, ...)

## Install

### Alpine Linux

Dependencies:

```sh
apk install sqlite sqlite-dev curl curl-dev gpsd gpsd-dev clang lld cmake ninja git
```

Clone and enter the repo:

```sh
git clone https://github.com/xarblu/loclogpp.git
cd loclogpp
```

Create build directory and compile the project:

```sh
mkdir build
cd build
cmake -DCMAKE_CXX_COMPILER=clang++ -GNinja ..
ninja -j1
```

Install the project:

```sh
ninja install
```
