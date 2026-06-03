# loclogpp
Simple GPSD based location logger written in C++

## Roadmap

- [x] Basic point recording
- [ ] Export database as GPX
- [ ] Sending points to Owntracks-like HTTP endpoint
- [-] Minimum displacement (only record point if X meters away from previous)
  - [ ] Configurable option
- [-] Minimum accuracy (only record point if accuracy below threshold)
  - [ ] Configurable option
- [ ] Stationary state detection
  - [ ] Reduce point frequency when stationary (configurable "heartbeat frequency"?)
- [ ] Dynamic minimum displacement detection (lower frequency on higher speed)
