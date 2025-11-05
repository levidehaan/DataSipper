# DataSipper - Network Monitoring Browser

DataSipper is a custom web browser based on Chromium that features a unique slide-out panel for monitoring real-time data streams like REST API calls and WebSocket connections. It provides deep insights into website data flows while maintaining a familiar browsing experience.

## 🚀 Quick Start

### Prerequisites
- **Arch Linux** (primary target platform)
- **8GB+ RAM** (16GB+ recommended)
- **100GB+ free disk space**
- **Fast internet connection** (Chromium source is ~10GB)

### One-Command Setup
```bash
# Clone the repository
git clone <repository-url>
cd datasipper

# Complete automated setup (1-4 hours depending on system)
./scripts/dev-setup.sh
```

### Alternative: Manual Setup
```bash
# Install dependencies
./scripts/install-deps-arch.sh

# Set up environment
source scripts/setup-env.sh

# Fetch Chromium source
./scripts/fetch-chromium.sh

# Apply DataSipper patches
cd chromium-src/src && python3 ../../scripts/patches.py apply

# Configure and build
cd ../.. && ./scripts/configure-build.sh dev
ninja -C chromium-src/src/out/DataSipper chrome

# Run DataSipper
./chromium-src/src/out/DataSipper/chrome
```

### Docker Development
```bash
# Build development container
docker build -f Dockerfile.dev --target development -t datasipper:dev .

# Run development environment
docker run -it --rm \
  -v $(pwd):/home/datasipper/datasipper \
  datasipper:dev

# Inside container
./scripts/dev-setup.sh
```

## 🔧 Current Implementation Status

### ✅ Completed
- **Development Environment**: Complete automated setup with Docker support
- **Stream Configuration UI**: Advanced routing rules with condition-based filtering
- **Rule Testing System**: Built-in testing with sample data and visual feedback
- **UI Framework**: Modern responsive interface with tabs, modals, and controls
- **Patch Management**: Full patch management system for Chromium modifications
- **Build System**: Optimized build configurations (debug, dev, release)

### 🚧 In Progress
- **Chromium Integration**: Setting up development environment and source fetch
- **Network Interception**: HTTP/HTTPS and WebSocket traffic capture implementation
- **Data Storage**: SQLite database and in-memory data structures

### 📋 Next Steps
- **Core Network Hooks**: URLLoader and WebSocket interception patches
- **IPC Communication**: Connect network observers to UI panel
- **Real-time Display**: Live stream of network events
- **External Integrations**: Kafka, Redis, MySQL output connectors

## 🏗️ Architecture

### Core Components

1. **Network Interception Layer**
   - `URLLoaderRequestInterceptor`: Captures HTTP/HTTPS traffic
   - `DataSipperNetworkObserver`: Processes request/response data
   - `DataSipperWebSocketObserver`: Monitors WebSocket connections

2. **Data Storage System**
   - `DataSipperDatabase`: SQLite-based persistent storage
   - `CircularEventBuffer`: In-memory real-time event queue
   - `NetworkEventStorage`: HTTP event management
   - `WebSocketMessageStorage`: WebSocket message handling

3. **User Interface**
   - Slide-out monitoring panel (integrated into browser UI)
   - Real-time data visualization
   - Filtering and search capabilities
   - Export functionality

## 🛠️ Development Workflow

### Making Changes

1. **Source Environment**
   ```bash
   source scripts/setup-env.sh
   source scripts/set_quilt_vars.sh
   cd chromium-src/src
   ```

2. **Create New Feature Patch**
   ```bash
   # Create patch
   qnew core/datasipper/my-feature.patch
   
   # Add files to patch
   qadd path/to/file.cc
   qadd path/to/file.h
   
   # Make changes
   qedit path/to/file.cc
   
   # Update patch
   qrefresh
   ```

3. **Build and Test**
   ```bash
   ninja -C out/DataSipper chrome
   ./out/DataSipper/chrome
   ```

### Patch Management

- **List patches**: `./scripts/patches.py list`
- **Apply patches**: `./scripts/patches.py apply`
- **Remove patches**: `./scripts/patches.py reverse`
- **Validate patches**: `./scripts/patches.py validate`

## 📁 Project Structure

```
datasipper/
├── build/                      # Build artifacts and depot_tools
├── chromium-src/              # Chromium source code
├── docs/                      # Documentation
│   ├── GETTING_STARTED.md     # Detailed setup guide
│   └── PATCH_DEVELOPMENT.md   # Patch development workflow
├── patches/                   # DataSipper modifications
│   ├── series                 # Patch application order
│   ├── core/                  # Essential functionality
│   │   ├── datasipper/        # Core infrastructure
│   │   ├── network-interception/ # Network capture
│   │   └── ui-panel/          # User interface
│   ├── extra/                 # Optional features
│   └── upstream-fixes/        # Chromium bug fixes
├── scripts/                   # Development tools
│   ├── setup-env.sh          # Environment configuration
│   ├── fetch-chromium.sh     # Source fetching
│   ├── configure-build.sh    # Build configuration
│   ├── patches.py            # Patch management
│   └── install-deps-arch.sh  # Dependency installation
└── todo.md                    # Detailed development roadmap
```

## 🎯 Key Features

### Network Monitoring
- **HTTP/HTTPS Interception**: Complete request/response capture
- **WebSocket Monitoring**: Bidirectional message logging
- **Real-time Display**: Live stream of network events
- **Historical Storage**: SQLite database for persistence

### Data Processing
- **Filtering**: By URL patterns, content type, method
- **Grouping**: By domain, API endpoint, content type
- **Search**: Full-text search across captured data
- **Export**: JSON, CSV formats

### Integration Capabilities
- **Kafka Producer**: Stream events to Kafka topics
- **MySQL Storage**: Direct database integration
- **Redis Caching**: High-performance data caching
- **Webhooks**: HTTP endpoint forwarding
- **JavaScript API**: Custom processing scripts

## 📚 Documentation

- **[Getting Started Guide](docs/GETTING_STARTED.md)**: Complete setup instructions
- **[Patch Development](docs/PATCH_DEVELOPMENT.md)**: How to modify Chromium
- **[Development Roadmap](todo.md)**: Detailed task breakdown

## 🔍 Current Network Interception

The current implementation captures:

### HTTP/HTTPS Traffic
- Request URL, method, headers, body
- Response status, headers, body
- Timing information
- Error codes and failure reasons

### WebSocket Connections
- Connection establishment and handshake
- Bidirectional message content (text/binary)
- Frame metadata (opcode, FIN bit)
- Connection closure events

### Data Storage
- Real-time circular buffer (10,000 events default)
- SQLite persistence with indexed queries
- Configurable retention policies
- Database maintenance and cleanup

## 🛡️ Security Considerations

- All captured data remains local by default
- Optional external forwarding requires explicit configuration
- Request/response bodies can be disabled for sensitive data
- Configurable data retention and cleanup policies

## 🤝 Contributing

1. Follow the patch development workflow in [PATCH_DEVELOPMENT.md](docs/PATCH_DEVELOPMENT.md)
2. Test thoroughly on clean Chromium source
3. Document changes and maintain compatibility
4. Submit patches for review

## 📄 License

DataSipper is built on Chromium and follows the same BSD-style license. See individual files for specific license information.

## 🏃‍♂️ Next Steps

Based on the current implementation status, the immediate priorities are:

1. **Complete UI Panel**: Implement the slide-out panel with React/JavaScript
2. **IPC Communication**: Connect network observers to UI panel
3. **Real-time Updates**: WebSocket/MessageChannel for live data
4. **Basic Filtering**: URL patterns and content type filters
5. **Export Functionality**: JSON/CSV export for captured data

The foundation is solid with working network interception and data storage. The next phase focuses on user interface and real-time data presentation.