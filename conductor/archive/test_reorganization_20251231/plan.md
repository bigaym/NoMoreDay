# Implementation Plan: Test Case Reorganization

## Phase 1: Setup Test Environment and Initial Restructuring
- [x] Task: Understand existing test structure and dependencies.
  - [x] Sub-task: Identify all existing test files in `tests/`.
  - [x] Sub-task: Analyze existing CMakeLists.txt in `tests/` for test compilation and linking.
  - [x] Sub-task: Determine current method of running tests (e.g., specific executables, `doctest` commands).
- [x] Task: Create a new main test runner file.
  - [x] Sub-task: Create `tests/main.cpp`.
  - [x] Sub-task: Add `doctest.h` include and `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` macro to `tests/main.cpp`.
- [x] Task: Create new test header files for each subsystem.
  - [x] Sub-task: For each existing test file (e.g., `CombatSystemTest.cpp`), create a corresponding header file (e.g., `CombatSystemTest.hpp`) in `tests/`.
  - [x] Sub-task: Move existing test cases from `.cpp` files to their respective new `.hpp` files.
  - [x] Sub-task: Modify original `.cpp` files to only include their corresponding `.hpp` files (or remove them if no other code is present).
- [x] Task: Update `tests/CMakeLists.txt` to compile the new main test runner.
  - [x] Sub-task: Modify CMake to compile `tests/main.cpp`.
  - [x] Sub-task: Ensure all new subsystem test header files are correctly included in the build process (e.g., through including them in `main.cpp`).
- [x] Task: Conductor - User Manual Verification 'Setup Test Environment and Initial Restructuring' (Protocol in workflow.md)

## Phase 2: Migration and Verification
- [x] Task: Migrate test cases to new header files.
  - [x] Sub-task: Iterate through each existing test `.cpp` file.
  - [x] Sub-task: Cut and paste `TEST_CASE` blocks into the corresponding new `.hpp` file.
  - [x] Sub-task: Ensure necessary includes are added to each `.hpp` file.
- [x] Task: Test compilation and execution of the reorganized tests.
  - [x] Sub-task: Build the project and verify no compilation errors related to test files.
  - [x] Sub-task: Run the main test executable and confirm all migrated tests execute.
  - [x] Sub-task: Verify all tests pass as expected.
- [x] Task: Confirm performance tests are unaffected.
  - [x] Sub-task: Run performance tests and verify they execute correctly and produce expected results.
- [x] Task: Remove old test `.cpp` files (if empty).
- [x] Task: Conductor - User Manual Verification 'Migration and Verification' (Protocol in workflow.md)

---
