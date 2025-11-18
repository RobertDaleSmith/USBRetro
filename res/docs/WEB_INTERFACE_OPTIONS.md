# Web Interface Options for USBRetro

This document outlines approaches for hosting a web-based configuration interface on the RP2040 board.

## Overview

A web interface would allow users to configure button mappings, profiles, thresholds, and other settings without recompiling firmware. This document evaluates four implementation approaches.

---

## Option 1: USB-Ethernet + Web Server ⭐ **Recommended**

The RP2040 presents itself as a USB Ethernet adapter and hosts a lightweight web server.

### Architecture
```
Computer USB ↔ RP2040 (USB Ethernet) ↔ lwIP Stack ↔ httpd ↔ Web UI
```

### Implementation Details

**Hardware:**
- No additional hardware required
- Uses existing USB connection

**Software Stack:**
- TinyUSB CDC-NCM or RNDIS class
- lwIP (Lightweight IP) - already in pico-sdk
- httpd (HTTP server from lwIP)
- LittleFS for config storage

**Network Configuration:**
- Static IP: 192.168.7.1/24
- mDNS: http://usbretro.local (optional)
- DHCP server for host computer

### Features

**Configuration Pages:**
- Profile management (create/edit/delete/select)
- Button mapping configurator
- Threshold and sensitivity sliders
- Real-time controller testing
- Firmware update interface
- Import/export JSON profiles

**API Endpoints:**
```
GET  /api/profiles        # List all profiles
POST /api/profiles        # Create new profile
GET  /api/profiles/:id    # Get profile by ID
PUT  /api/profiles/:id    # Update profile
DELETE /api/profiles/:id  # Delete profile

GET  /api/config          # Get current configuration
POST /api/config          # Update configuration
POST /api/save            # Save to flash

GET  /api/status          # Device status (controller count, active profile, etc.)
GET  /api/controller      # Real-time controller input data
```

### Resource Requirements

- **Flash**: ~50-80KB (lwIP + httpd + web files)
- **RAM**: ~40KB (lwIP buffers + HTTP)
- **Development Time**: ~2-3 weeks

### Pros

✓ Full web interface works in any browser
✓ Professional user experience
✓ Easy to add features (diagnostics, testing, updates)
✓ No browser restrictions
✓ Standard HTTP/REST API
✓ Works with command-line tools (curl, wget)

### Cons

✗ Moderate flash/RAM overhead
✗ More complex implementation
✗ Requires network stack

### Use Cases

- Configure button mappings via dropdown menus
- Create custom profiles with visual editor
- Upload/download JSON config files
- Real-time controller input debugging
- Firmware updates via web browser
- Save profiles to flash with one click

---

## Option 2: WebUSB ⚡ **Lighter Weight**

Browser communicates directly with RP2040 via WebUSB API (no network stack needed).

### Architecture
```
Browser WebUSB API ↔ USB Control Transfers ↔ RP2040
```

### Implementation Details

**Hardware:**
- No additional hardware required

**Software Stack:**
- TinyUSB with WebUSB descriptors
- Custom USB vendor interface
- Web page (hosted externally or on GitHub Pages)

**Web Page Requirements:**
- Must be HTTPS or localhost
- Uses WebUSB JavaScript API
- Direct USB communication

### Resource Requirements

- **Flash**: ~5KB (WebUSB descriptors + handlers)
- **RAM**: ~2KB (USB buffers)
- **Development Time**: ~1 week

### Pros

✓ Minimal flash/RAM overhead
✓ No network stack required
✓ Very fast response times
✓ Direct USB communication
✓ Simple protocol design

### Cons

✗ Only works in Chromium browsers (Chrome, Edge, Opera)
✗ Web page must be HTTPS or localhost
✗ User must manually grant USB permissions
✗ More limited than full web server

### Use Cases

- Quick configuration tool
- Embedded devices with limited resources
- Developer-focused interface

---

## Option 3: USB Mass Storage + HTML

RP2040 presents as USB drive with HTML files stored in flash.

### Architecture
```
Computer ↔ USB MSC ↔ LittleFS ↔ HTML/JS Files
Browser ↔ WebUSB ↔ RP2040 (for bidirectional comms)
```

### Implementation Details

**Hardware:**
- No additional hardware required

**Software Stack:**
- TinyUSB MSC (Mass Storage Class)
- LittleFS filesystem
- WebUSB for configuration changes

**User Experience:**
1. Plug in device
2. Drive appears with index.html
3. Open index.html in browser
4. Configure via web UI
5. Changes saved via WebUSB

### Resource Requirements

- **Flash**: ~20-30KB (LittleFS + MSC + WebUSB)
- **RAM**: ~8KB (MSC buffers)
- **Development Time**: ~1-2 weeks

### Pros

✓ Easy to update (edit HTML files directly)
✓ No driver installation needed
✓ User-friendly (just open drive)
✓ HTML files version-controlled

### Cons

✗ Requires WebUSB for bidirectional communication
✗ Tricky to implement reliable MSC with config storage
✗ File system corruption risk
✗ OS caching issues

---

## Option 4: WebSerial 🔧 **Simplest**

Web page uses WebSerial API to communicate via USB CDC (serial).

### Architecture
```
Browser WebSerial API ↔ USB CDC ↔ RP2040 (printf/scanf)
```

### Implementation Details

**Hardware:**
- No additional hardware required

**Software Stack:**
- Existing USB CDC (already in TinyUSB)
- JSON protocol over serial
- Web page uses WebSerial API

**Protocol Example:**
```json
// Request
{"cmd": "get_profile", "id": 0}

// Response
{"status": "ok", "profile": {...}}

// Request
{"cmd": "set_profile", "profile": {...}}

// Response
{"status": "ok"}
```

### Resource Requirements

- **Flash**: ~2KB (JSON parser)
- **RAM**: ~1KB (buffers)
- **Development Time**: ~3-5 days

### Pros

✓ Minimal code changes
✓ Very small overhead
✓ Simple text-based protocol
✓ Easy to debug (serial monitor)

### Cons

✗ Chromium browsers only
✗ Less polished UX
✗ Serial port conflicts with debug output
✗ No visual feedback on device

---

## Comparison Matrix

| Feature | USB-Ethernet | WebUSB | USB MSC | WebSerial |
|---------|--------------|---------|---------|-----------|
| **Flash Usage** | 50-80KB | 5KB | 20-30KB | 2KB |
| **RAM Usage** | 40KB | 2KB | 8KB | 1KB |
| **Browser Support** | All | Chromium | Chromium | Chromium |
| **UX Quality** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| **Development Time** | 2-3 weeks | 1 week | 1-2 weeks | 3-5 days |
| **Complexity** | High | Medium | Medium | Low |
| **Features** | Full | Good | Limited | Basic |

---

## Recommendation

**Start with Option 1 (USB-Ethernet + Web Server)** for production devices because:

1. **Best User Experience**: Works in any browser, no permission prompts
2. **Professional**: Expected by users for modern devices
3. **Future-Proof**: Easy to add features (diagnostics, testing, firmware updates)
4. **Standard**: Uses HTTP/REST, works with any HTTP client
5. **Well-Supported**: lwIP is mature and already in pico-sdk

**Use Option 4 (WebSerial)** for rapid prototyping or developer tools.

---

## Implementation Roadmap

### Phase 1: Basic Web Server (Week 1)
- [ ] Enable USB Ethernet (ECM/RNDIS)
- [ ] Initialize lwIP stack
- [ ] Basic httpd with static pages
- [ ] Test connectivity

### Phase 2: Configuration API (Week 2)
- [ ] Profile storage in flash (LittleFS)
- [ ] REST API endpoints
- [ ] JSON serialization/deserialization
- [ ] Save/load functionality

### Phase 3: Web UI (Week 3)
- [ ] Profile selector
- [ ] Button mapping configurator
- [ ] Threshold sliders
- [ ] Real-time testing
- [ ] Import/export

### Phase 4: Advanced Features (Future)
- [ ] Firmware update via web
- [ ] Controller diagnostics
- [ ] Usage statistics
- [ ] Profile sharing community

---

## Security Considerations

### USB-Ethernet Approach
- No internet access (only local USB network)
- No external attack surface
- Optional: Basic authentication for web interface
- Consider: Signed firmware updates

### WebUSB Approach
- Browser same-origin policy applies
- User must explicitly grant USB permissions
- HTTPS required for hosted pages
- Consider: Code signing for web page

---

## References

- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [lwIP Documentation](https://www.nongnu.org/lwip/)
- [WebUSB Specification](https://wicg.github.io/webusb/)
- [WebSerial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)

---

## Future Enhancements

### Advanced Configuration
- Macro recording/playback
- Turbo button configuration
- Dead zone adjustment
- Stick response curves

### Diagnostics
- Input latency measurement
- Button press histogram
- Analog value graphs
- USB polling rate display

### Community Features
- Profile sharing (QR codes or URLs)
- Popular profile library
- User ratings and comments
- Automatic profile updates
