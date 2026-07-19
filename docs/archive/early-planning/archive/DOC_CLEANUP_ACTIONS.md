# Documentation Cleanup Actions

**Action Required**: Execute before Stage 1
**Purpose**: Archive obsolete documentation, keep repository clean
**Status**: Ready to execute

---

## Overview

The docs/ directory contains 14 obsolete files from the folly removal and planning phases. These files are historical and will be archived to `docs/archive/` for reference while keeping the active documentation clean and focused.

---

## Files to Archive (14 total)

### Planning & Historical Documents
1. `DEPENDENCY_STRATEGY.md` - Obsolete (folly removed)
2. `DOCS_CLEANUP.md` - Superseded by this file
3. `FINAL_SOLUTION.md` - Historical folly removal notes
4. `FOLLY_REMOVAL_SUMMARY.md` - Historical summary
5. `MASTER_PLAN.md` - Superseded by IMPLEMENTATION_PLAN.md
6. `NEXT_STEPS.md` - Superseded by implementation plans
7. `PURPOSE_AND_SCOPE.md` - Historical planning
8. `REGRESSION_PREVENTION.md` - Historical testing notes
9. `remove-folly-plan.md` - Historical folly removal plan
10. `remove-folly-status.md` - Historical status tracking
11. `RENAMING_PROPOSAL.md` - Superseded by STAGE_1_IMPLEMENTATION.md
12. `STAGE_1_PLAN.md` - Superseded by STAGE_1_IMPLEMENTATION.md
13. `STATIC_LINKING_STRATEGY.md` - Historical strategy notes
14. `TEBAKO_VFS_ARCHITECTURE.md` - Superseded by ARCHITECTURE.md
15. `TEST_RESULTS.md` - Historical test results
16. `TESTING_PLAN.md` - Superseded by TESTING_STRATEGY.md

---

## Files to Keep (10 total)

### Active Documentation
1. `ARCHITECTURE.md` - Current system architecture
2. `ARCHIVE_FORMATS_AS_FILESYSTEMS.md` - Technical reference
3. `MULTI_LANGUAGE_ARCHITECTURE.md` - Future expansion guide
4. `IMPLEMENTATION_PLAN.md` - Main execution plan (NEW)
5. `STAGE_1_IMPLEMENTATION.md` - Stage 1 details (NEW)
6. `STAGE_2_IMPLEMENTATION.md` - Stage 2 details (NEW)
7. `STAGE_3_IMPLEMENTATION.md` - Stage 3 details (NEW)
8. `DOC_CLEANUP_ACTIONS.md` - This file (NEW)
9. `TESTING_STRATEGY.md` - Testing approach (NEW)
10. `FLATBUFFERS_MIGRATION.md` - Serialization guide (NEW)

---

## Cleanup Script

Execute this script from the repository root:

```bash
#!/bin/bash
# docs-cleanup.sh - Archive obsolete documentation

set -e

REPO_ROOT="/Users/mulgogi/src/tamatebako/libdwarfs"
DOCS_DIR="${REPO_ROOT}/docs"
ARCHIVE_DIR="${DOCS_DIR}/archive"

echo "Documentation Cleanup Script"
echo "============================"
echo ""

# Create archive directory
mkdir -p "${ARCHIVE_DIR}"
echo "✓ Created archive directory: ${ARCHIVE_DIR}"

# Files to archive
FILES_TO_ARCHIVE=(
    "DEPENDENCY_STRATEGY.md"
    "DOCS_CLEANUP.md"
    "FINAL_SOLUTION.md"
    "FOLLY_REMOVAL_SUMMARY.md"
    "MASTER_PLAN.md"
    "NEXT_STEPS.md"
    "PURPOSE_AND_SCOPE.md"
    "REGRESSION_PREVENTION.md"
    "remove-folly-plan.md"
    "remove-folly-status.md"
    "RENAMING_PROPOSAL.md"
    "STAGE_1_PLAN.md"
    "STATIC_LINKING_STRATEGY.md"
    "TEBAKO_VFS_ARCHITECTURE.md"
    "TEST_RESULTS.md"
    "TESTING_PLAN.md"
)

# Move files to archive
echo ""
echo "Archiving obsolete documentation..."
for file in "${FILES_TO_ARCHIVE[@]}"; do
    if [ -f "${DOCS_DIR}/${file}" ]; then
        git mv "${DOCS_DIR}/${file}" "${ARCHIVE_DIR}/${file}"
        echo "  ✓ Archived: ${file}"
    else
        echo "  ⚠ Not found: ${file}"
    fi
done

# Create archive README
cat > "${ARCHIVE_DIR}/README.md" << 'EOF'
# Archived Documentation

This directory contains historical documentation from the libdwarfs-wr to libtfs transformation project.

## Contents

### Folly Removal (2024-2025)
- Planning documents for removing Folly dependency
- Implementation status and test results
- Static linking strategy development

### Transformation Planning
- Early proposals for renaming and restructuring
- Original stage 1 plans (superseded)
- Historical architecture discussions

## Relevance

These documents are preserved for historical reference but are no longer actively maintained. For current documentation, see the parent docs/ directory.

**Archived**: 2025-01-17
**Project**: libtfs (formerly libdwarfs-wr)
EOF

echo ""
echo "✓ Created archive README"

# Verify active documentation
echo ""
echo "Active documentation:"
ls -1 "${DOCS_DIR}"/*.md | grep -v archive | sort

echo ""
echo "Archived documentation:"
ls -1 "${ARCHIVE_DIR}"/*.md | sort

echo ""
echo "═══════════════════════════════════"
echo "Cleanup complete!"
echo "═══════════════════════════════════"
echo ""
echo "Next steps:"
echo "  1. Review changes: git status"
echo "  2. Commit: git commit -m 'docs: archive obsolete documentation'"
echo "  3. Proceed to Stage 1: docs/STAGE_1_IMPLEMENTATION.md"
```

---

## Execution Steps

### 1. Create and Run Script
```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs

# Create script
cat > docs-cleanup.sh << 'EOF'
[paste script from above]
EOF

# Make executable
chmod +x docs-cleanup.sh

# Run
./docs-cleanup.sh
```

### 2. Review Changes
```bash
git status
git diff --cached
```

### 3. Commit
```bash
git add docs/archive/
git commit -m "docs: archive obsolete documentation

- Moved 16 historical docs to docs/archive/
- Kept 10 active documentation files
- Added archive README
- Cleaned up post-folly-removal artifacts

Prepares repository for Stage 1 (rename to libtfs).
"
```

---

## Verification

After execution, verify:

```bash
# Should list ~10 active docs
ls -1 docs/*.md | wc -l

# Should list 16 archived docs
ls -1 docs/archive/*.md | wc -l

# Should show clean structure
tree docs/ -L 2
```

Expected output:
```
docs/
├── archive/
│   ├── README.md
│   ├── [16 archived .md files]
├── ARCHITECTURE.md
├── ARCHIVE_FORMATS_AS_FILESYSTEMS.md
├── DOC_CLEANUP_ACTIONS.md
├── FLATBUFFERS_MIGRATION.md
├── IMPLEMENTATION_PLAN.md
├── MULTI_LANGUAGE_ARCHITECTURE.md
├── STAGE_1_IMPLEMENTATION.md
├── STAGE_2_IMPLEMENTATION.md
├── STAGE_3_IMPLEMENTATION.md
└── TESTING_STRATEGY.md
```

---

## Rationale

### Why Archive (Not Delete)?

1. **Historical Reference**: Future contributors may want to understand decision-making process
2. **Git History**: Deletion complicates git blame and history tracking
3. **Audit Trail**: Demonstrates due diligence in dependency removal
4. **Learning Resource**: Shows evolution of project architecture

### Why Not Keep Active?

1. **Confusion**: Outdated information misleads new contributors
2. **Maintenance**: Obsolete docs don't get updated
3. **Signal-to-Noise**: Hard to find current documentation
4. **Professionalism**: Clean docs show project maturity

---

## Post-Cleanup

After cleanup:
- Repository has clear, focused documentation
- No conflicting or obsolete information
- Ready for Stage 1 execution
- Contributors see only current, relevant docs

---

**Execute this cleanup before starting Stage 1!**

Last Updated: 2025-01-17