# libtfs Roadmap

## Current Release: v0.12.0 (2026-07-21)

**Status**: ✅ Production Ready

- 100% test pass rate (187/187 tests: 140 ZIP + 47 DwarFS)
- Modern C API with Ruby FFI compatibility
- ZIP backend fully functional
- **DwarFS backend fully functional** (NEW!)
- Comprehensive documentation
- Performance baseline established

## v0.12.0 (Current Release - Q1 2026)

### Priority 1: Core Features
- [x] **DwarFS Backend Implementation** ✅ DONE
  - [x] Implement DwarfsBackend class
  - [x] Add DwarFS-specific tests (47 tests passing)
  - [x] Performance benchmarks vs ZIP
  - [x] Documentation

- [x] **Extraction Functionality** ✅ DONE
  - [x] Implement tebako_fs_extract_all()
  - [x] Recursive directory extraction
  - [x] Permission preservation
  - [x] Timestamp preservation
  - [x] 23 tests passing
  - [ ] Progress callbacks (optional enhancement)

### Priority 2: Performance
- [ ] **Directory Caching**
  - Cache directory listings
  - Invalidation strategy
  - Memory management

- [ ] **Read Buffering**
  - Larger internal buffers
  - Sequential read optimization
  - Benchmark improvements

### Priority 3: Testing
- [ ] **Cross-Platform Benchmarks**
  - Linux performance baseline
  - Windows performance baseline
  - Comparison reports

- [ ] **Stress Testing**
  - Large archive support (multi-GB)
  - Sustained operations testing
  - Memory leak detection

## v0.13.0 (Q2 2026)

### New Backends
- [ ] **TAR Backend**
  - Basic TAR support
  - Compression variants (gz, bz2, xz)
  - Streaming support

- [ ] **ISO 9660 Backend** (tentative)
  - Read-only ISO support
  - Rock Ridge extensions
  - Joliet extensions

### Advanced Features
- [ ] **Write Operations** (optional)
  - Archive modification support
  - Safety mechanisms
  - Transaction support

- [ ] **Compression Options**
  - Configurable compression levels
  - Algorithm selection
  - Trade-off profiles

## v1.0.0 (Long-term - 2026)

### API Stability
- [ ] **Stable API**
  - Version compatibility guarantees
  - Deprecation policy
  - Migration guides

### Platform Coverage
- [ ] **Full Platform Matrix**
  - macOS (Intel + Apple Silicon)
  - Linux (x86_64 + aarch64)
  - Windows (MSVC + MinGW)
  - BSD variants

### Performance
- [ ] **Optimization**
  - Parallel decompression
  - Metadata indexing
  - Memory-mapped I/O (where possible)

### Quality
- [ ] **Complete Automation**
  - Full CI/CD pipeline
  - Automated performance regression
  - Automated documentation generation
  - Release automation

## Future Considerations

### Multi-Archive Support
- Multiple simultaneous mounts
- Archive overlays
- Namespace management

### Advanced Features
- Symbolic link support (full)
- Extended attributes
- FUSE integration (optional)
- Network archive support

### Integration
- Python bindings
- Node.js bindings
- Go bindings
- Rust bindings

## Contributing

Interested in contributing to the roadmap?

1. Check existing issues/PRs
2. Discuss proposals in GitHub Discussions
3. Follow contribution guidelines
4. Submit well-tested PRs

## Release Schedule

- **Minor releases** (x.Y.0): Quarterly
- **Patch releases** (x.y.Z): As needed
- **Major releases** (X.0.0): Annually

## Version Support

- **Current**: Full support
- **Previous minor**: Security fixes
- **Older**: Best effort

## Contact

- GitHub Issues: Bug reports and feature requests
- GitHub Discussions: General questions and proposals
- Email: [project contact]

---

**Last Updated**: 2026-07-21
**Next Review**: v0.13.0 release