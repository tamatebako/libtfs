# libtfs Documentation

## Active Documentation

### Core Documentation
- [`README.adoc`](../README.adoc) - Main project documentation
- [`ARCHITECTURE.md`](ARCHITECTURE.md) - System architecture and SOLID principles
- [`TESTING.adoc`](TESTING.adoc) - Comprehensive testing guide

### Backend Documentation
- [`ZIP_BACKEND.adoc`](backends/ZIP_BACKEND.adoc) - ZIP archive backend
- [`SQUASHFS_BACKEND.adoc`](backends/SQUASHFS_BACKEND.adoc) - SquashFS backend

### Performance & Integration
- [`PERFORMANCE_BASELINE.md`](PERFORMANCE_BASELINE.md) - Performance metrics and regression criteria
- [`TEBAKO_INTEGRATION.md`](TEBAKO_INTEGRATION.md) - Tebako integration guide *(Phase 5)*
- [`DEPLOYMENT_CHECKLIST.md`](DEPLOYMENT_CHECKLIST.md) - Deployment validation *(Phase 5)*

### Release Documentation
- [`RELEASE_NOTES.md`](../RELEASE_NOTES.md) - v0.11.0 release notes
- [`CHANGELOG.md`](../CHANGELOG.md) - Version history
- [`ROADMAP.md`](../ROADMAP.md) - Future development plans *(Phase 5)*

## Archived Documentation

### Phase Completion Documentation
- [Phase 4: Production Readiness](../old-docs/phase4_completed/) - Quality validation and code cleanup
- [Phase 3: Testing & Validation](../old-docs/dwarfs_v09_completed/) - Comprehensive test suite
- [Stage 2: Multi-Backend VFS](archive/) - VFS abstraction layer
- [Stage 1: FlatBuffers Migration](archive/) - Modernization foundation

## Documentation Standards

### Format Guidelines
- **User-facing documentation**: AsciiDoc format (`.adoc`)
- **Development documentation**: Markdown format (`.md`)
- **Code documentation**: Inline Doxygen comments

### Organization
- **Active docs**: Keep in `docs/` directory
- **Completed phases**: Archive to `old-docs/{phase_name}/`
- **Historical reference**: Store in `docs/archive/`

### Maintenance
- Keep [`README.adoc`](../README.adoc) as single source of project truth
- Update this index when adding new documentation
- Archive completed phase documents promptly
- Review and update quarterly

## Documentation by Topic

### Getting Started
1. Read [`README.adoc`](../README.adoc) for project overview
2. Check [`backends/ZIP_BACKEND.adoc`](backends/ZIP_BACKEND.adoc) for backend details
3. Follow [`TESTING.adoc`](TESTING.adoc) to run tests
4. Review [`TEBAKO_INTEGRATION.md`](TEBAKO_INTEGRATION.md) for integration

### Development
1. Understand [`ARCHITECTURE.md`](ARCHITECTURE.md) for design principles
2. Check [`TESTING.adoc`](TESTING.adoc) for testing requirements
3. Review [`PERFORMANCE_BASELINE.md`](PERFORMANCE_BASELINE.md) for metrics
4. Follow code style in existing sources

### Deployment
1. Validate with [`DEPLOYMENT_CHECKLIST.md`](DEPLOYMENT_CHECKLIST.md)
2. Check [`PERFORMANCE_BASELINE.md`](PERFORMANCE_BASELINE.md) for regression
3. Review [`RELEASE_NOTES.md`](../RELEASE_NOTES.md) for changes
4. Follow [`TEBAKO_INTEGRATION.md`](TEBAKO_INTEGRATION.md) for integration

### Future Planning
1. Review [`ROADMAP.md`](../ROADMAP.md) for upcoming features
2. Check [`CHANGELOG.md`](../CHANGELOG.md) for version history
3. Consult archived phases for historical context

## Contributing

When contributing documentation:
- Follow the format guidelines above
- Update this index when adding new docs
- Keep documentation clear and concise
- Include code examples where helpful
- Test all code examples before committing

## Questions?

- Check existing documentation first
- Review archived phases for historical context
- Consult [`ARCHITECTURE.md`](ARCHITECTURE.md) for design decisions
- See [`TESTING.adoc`](TESTING.adoc) for test guidance

---

**Last Updated**: 2025-12-24 (Phase 5)
**Documentation Version**: v0.11.0