# loclogpp
Simple GPSD based location logger written in C++

## Roadmap

- [ ] Basic point recording
- [ ] Sending points to Owntracks-like HTTP endpoint
- [ ] Minimum displacement (only record point if X meters away from previous)
- [ ] Minimum accuracy (only record point if accuracy below threshold)
- [ ] Stationary state detection
  - [ ] Reduce point frequency when stationary (configurable "heartbeat frequency"?)
- [ ] Dynamic minimum displacement detection (lower frequency on higher speed)
