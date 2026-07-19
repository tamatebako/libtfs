# Archive Instructions

**Date**: 2025-12-22  
**Action Required**: Move completed status documents to archive

---

## Files to Archive

The following completed status documents should be moved to `docs/archive/`:

### Stage 2 Day Status Files (Completed Week 1)

```bash
# Move these files to docs/archive/
mv docs/STAGE_2_DAY1_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY2_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY3_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY4_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY5_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY6_COMPLETION_STATUS.md docs/archive/
```

### Old Continuation Prompts

```bash
# Move old continuation prompts to docs/archive/
mv docs/STAGE_2_DAY4_CONTINUATION_PROMPT.md docs/archive/
mv docs/STAGE_2_DAY5_CONTINUATION_PROMPT.md docs/archive/
mv docs/STAGE_2_DAY6_CONTINUATION_PROMPT.md docs/archive/
```

---

## Reason for Archiving

These documents represent completed work from Stage 2 Week 1:
- Days 1-6 are complete
- ZIP backend is production-ready
- SquashFS backend is production-ready
- CLI tool is complete

We're now moving to Week 2+ (6-8 week plan for production readiness).

---

## Active Documents (Keep in docs/)

The following documents should remain in `docs/`:

### Current Planning Documents
- `STAGE_2_WEEK2_CONTINUATION_PLAN.md` - Detailed 6-8 week plan
- `STAGE_2_WEEK2_STATUS_TRACKER.md` - Living status tracker
- `STAGE_2_WEEK2_CONTINUATION_PROMPT.md` - C API implementation prompt

### Reference Documents
- `PRODUCTION_READINESS_CHECKLIST.md` - Complete 60-item checklist
- `TEBAKO_INTEGRATION_ARCHITECTURE.md` - Integration architecture
- `STAGE_2_VFS_DESIGN.md` - VFS architecture
- `STAGE_2_IMPLEMENTATION_STATUS_TRACKER.md` - Overall progress (update this)
- `TESTING.adoc` - Testing guide
- `README.adoc` - Main documentation

### Backend Documentation
- `docs/backends/ZIP_BACKEND.adoc`
- `docs/backends/SQUASHFS_BACKEND.adoc`

---

## Commands to Execute

```bash
# From repository root
cd /Users/mulgogi/src/tamatebako/libdwarfs

# Archive completed day status files
mv docs/STAGE_2_DAY1_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY2_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY3_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY4_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY5_COMPLETION_STATUS.md docs/archive/
mv docs/STAGE_2_DAY6_COMPLETION_STATUS.md docs/archive/

# Archive old continuation prompts
mv docs/STAGE_2_DAY4_CONTINUATION_PROMPT.md docs/archive/
mv docs/STAGE_2_DAY5_CONTINUATION_PROMPT.md docs/archive/
mv docs/STAGE_2_DAY6_CONTINUATION_PROMPT.md docs/archive/

# Verify archive
ls docs/archive/STAGE_2_DAY*.md

# Clean up this instruction file after archiving
rm docs/ARCHIVE_INSTRUCTIONS.md
```

---

**After archiving, your docs/ directory structure will be:**

```
docs/
├── PRODUCTION_READINESS_CHECKLIST.md (main checklist)
├── STAGE_2_WEEK2_CONTINUATION_PLAN.md (current plan)
├── STAGE_2_WEEK2_STATUS_TRACKER.md (living tracker)
├── STAGE_2_WEEK2_CONTINUATION_PROMPT.md (C API prompt)
├── TEBAKO_INTEGRATION_ARCHITECTURE.md (integration design)
├── STAGE_2_VFS_DESIGN.md (VFS architecture)
├── TESTING.adoc (testing guide)
├── archive/
│   ├── STAGE_2_DAY1_COMPLETION_STATUS.md ← ARCHIVED
│   ├── STAGE_2_DAY2_COMPLETION_STATUS.md ← ARCHIVED
│   ├── STAGE_2_DAY3_COMPLETION_STATUS.md ← ARCHIVED
│   ├── STAGE_2_DAY4_COMPLETION_STATUS.md ← ARCHIVED
│   ├── STAGE_2_DAY5_COMPLETION_STATUS.md ← ARCHIVED
│   ├── STAGE_2_DAY6_COMPLETION_STATUS.md ← ARCHIVED
│   ├── STAGE_2_DAY4_CONTINUATION_PROMPT.md ← ARCHIVED
│   ├── STAGE_2_DAY5_CONTINUATION_PROMPT.md ← ARCHIVED
│   └── STAGE_2_DAY6_CONTINUATION_PROMPT.md ← ARCHIVED
└── backends/
    ├── ZIP_BACKEND.adoc
    └── SQUASHFS_BACKEND.adoc