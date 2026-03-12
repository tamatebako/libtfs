# Documentation Cleanup Guide

## Files to KEEP (4 core documents)

### 1. [`MASTER_PLAN.md`](MASTER_PLAN.md)
**Purpose:** Overall transformation roadmap (3 stages)
**Status:** ✅ Current master plan

### 2. [`STAGE_1_PLAN.md`](STAGE_1_PLAN.md)
**Purpose:** Detailed execution plan for Stage 1 (rename & solidify)
**Status:** ✅ Active execution guide

### 3. [`ARCHITECTURE.md`](ARCHITECTURE.md)
**Purpose:** Core design decisions (jemalloc, separation, naming)
**Status:** ✅ Permanent reference

### 4. [`ARCHIVE_FORMATS_AS_FILESYSTEMS.md`](ARCHIVE_FORMATS_AS_FILESYSTEMS.md)
**Purpose:** ZIP/miniz backend implementation (Stage 2)
**Status:** ✅ Needed for Stage 2

### 5. [`MULTI_LANGUAGE_ARCHITECTURE.md`](MULTI_LANGUAGE_ARCHITECTURE.md)
**Purpose:** Julia/Python support plan (Stage 3)
**Status:** ✅ Needed for Stage 3

**Total to keep: 5 documents**

---

## Files to DELETE (13 obsolete documents)

### Historical Planning (No Longer Needed)

```bash
rm docs/remove-folly-plan.md              # Historical - tasks complete
rm docs/remove-folly-status.md            # Obsolete status tracking
rm docs/FOLLY_REMOVAL_SUMMARY.md         # Historical summary
rm docs/NEXT_STEPS.md                     # Replaced by MASTER_PLAN.md
```

### Superseded Documentation

```bash
rm docs/DEPENDENCY_STRATEGY.md            # Covered in ARCHITECTURE.md
rm docs/PURPOSE_AND_SCOPE.md              # Merged into ARCHITECTURE.md
rm docs/TEBAKO_VFS_ARCHITECTURE.md        # Merged into ARCHITECTURE.md
rm docs/RENAMING_PROPOSAL.md              # Approved, executing in Stage 1
rm docs/FINAL_SOLUTION.md                 # Historical, superseded by MASTER_PLAN
```

### Completed Strategy Documents

```bash
rm docs/STATIC_LINKING_STRATEGY.md        # Problem solved, now obsolete
rm docs/TESTING_PLAN.md                   # Testing framework now in CI/CD
rm docs/TEST_RESULTS.md                   # Historical test run
rm docs/REGRESSION_PREVENTION.md          # Implemented in GitHub Actions
```

**Total to delete: 13 documents**

---

## Cleanup Script

```bash
#!/bin/bash
# Run from project root

cd docs/

# Delete obsolete files
rm -f remove-folly-plan.md \
      remove-folly-status.md \
      FOLLY_REMOVAL_SUMMARY.md \
      NEXT_STEPS.md \
      DEPENDENCY_STRATEGY.md \
      PURPOSE_AND_SCOPE.md \
      TEBAKO_VFS_ARCHITECTURE.md \
      RENAMING_PROPOSAL.md \
      FINAL_SOLUTION.md \
      STATIC_LINKING_STRATEGY.md \
      TESTING_PLAN.md \
      TEST_RESULTS.md \
      REGRESSION_PREVENTION.md

# Verify only 5 docs remain
ls -1 *.md
# Expected output:
#   ARCHITECTURE.md
#   ARCHIVE_FORMATS_AS_FILESYSTEMS.md
#   DOCS_CLEANUP.md
#   MASTER_PLAN.md
#   MULTI_LANGUAGE_ARCHITECTURE.md
#   STAGE_1_PLAN.md

echo "✓ Documentation cleanup complete"
echo "Kept: 5 core documents"
echo "Removed: 13 obsolete documents"
```

---

## Git Cleanup

### Archive Deleted Docs (Keep in History)

```bash
# Before deleting, commit current state
git add docs/
git commit -m "docs: archive pre-cleanup state"

# Delete obsolete docs
./cleanup_docs.sh

# Commit cleanup
git add docs/
git commit -m "docs: remove obsolete planning documents

Removed 13 historical planning documents:
- remove-folly-* (3 files) - Tasks complete
- DEPENDENCY/PURPOSE/VFS docs - Merged into ARCHITECTURE.md
- Testing/prevention docs - Now in CI/CD workflows
- Strategy docs - Problems solved

Kept 5 core documents:
- MASTER_PLAN.md - Overall roadmap
- STAGE_1_PLAN.md - Current execution plan
- ARCHITECTURE.md - Design decisions
- ARCHIVE_FORMATS_AS_FILESYSTEMS.md - ZIP backend (Stage 2)
- MULTI_LANGUAGE_ARCHITECTURE.md - Multi-lang (Stage 3)"
```

### Maintaining History

**Important:** Files are deleted from working tree but remain in git history.

To view old documentation:
```bash
# List files in previous commit
git ls-tree HEAD~1 docs/

# View deleted file
git show HEAD~1:docs/FINAL_SOLUTION.md
```

---

## Final Documentation Structure

```
docs/
├── MASTER_PLAN.md                      # Overall 3-stage roadmap
├── STAGE_1_PLAN.md                     # Current stage (rename & solidify)
├── ARCHITECTURE.md                     # Core design decisions
├── ARCHIVE_FORMATS_AS_FILESYSTEMS.md   # ZIP backend (Stage 2)
├── MULTI_LANGUAGE_ARCHITECTURE.md      # Julia support (Stage 3)
└── DOCS_CLEANUP.md                     # This file (cleanup guide)
```

**Total: 6 files** (5 permanent + 1 cleanup guide)

Clean, focused, actionable.

---

## Rationale for Keeping Each Doc

**MASTER_PLAN.md:**
- Active roadmap for all 3 stages
- Reference for overall strategy
- Timeline and milestones

**STAGE_1_PLAN.md:**
- Day-by-day execution plan
- Checklist for current work
- Success criteria

**ARCHITECTURE.md:**
- Permanent design reference
- Answers "why" questions
- Design decisions documented

**ARCHIVE_FORMATS_AS_FILESYSTEMS.md:**
- Technical reference for Stage 2
- miniz integration details
- ZIP backend impl

ementation

**MULTI_LANGUAGE_ARCHITECTURE.md:**
- Technical reference for Stage 3
- Julia/Python support plan
- Adapter architecture

---

## After Cleanup

**Documentation is:**
- ✅ Focused (only actively used docs)
- ✅ Current (no obsolete content)
- ✅ Organized (one doc per purpose)
- ✅ Actionable (execution-ready)

**Eliminated:**
- ❌ Historical planning documents
- ❌ Completed task summaries
- ❌ Obsolete strategies
- ❌ Redundant content

**Result:** Clean, maintainable documentation set ready for Stage 1 execution.