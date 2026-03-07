#include "rvm/RVMInstruction.h"
#include "rvm/RVMLiveAnalyzer.h"
#include "rvm/RVMProgram.h"
#include "rvm/RVMSerializer.h"
#include <catch2/catch_test_macros.hpp>

using namespace PExpr::rvm;

namespace {
RVMProgram deserializeSafe(const std::string& ir)
{
    auto prog_opt = RVMSerializer::deserialize(ir);
    REQUIRE(prog_opt.has_value());
    return *prog_opt;
}
} // namespace

TEST_CASE("RVMLiveAnalyzer: Basic block with moves and non-moves", "[rvm][live_analyzer]")
{
    std::string rvmIr  = R"(
        mov %r0:int 42:int
        mov %r1:int %r0:int
        add %r2:int %r1:int 10:int
    )";
    RVMProgram program = deserializeSafe(rvmIr);

    auto intervals = RVMLiveAnalyzer::analyzeBlock(program);

    REQUIRE(intervals.size() == 3);

    auto getInterval = [&](RegId reg) {
        for (const auto& i : intervals) {
            if (i.Register == reg)
                return i;
        }
        throw std::runtime_error("Interval not found");
    };

    auto i0 = getInterval(0);
    CHECK(i0.Start == 0);
    // Interval for r0 goes from definition at 0 to move at 1,
    // but analyzeBlock currently lets dangling intervals go to the end of block (2).
    CHECK(i0.End == 2);
    CHECK(i0.HasNonMoveUsage == false);
    CHECK(i0.isPinned() == false);

    auto i1 = getInterval(1);
    CHECK(i1.Start == 1);
    CHECK(i1.End == 2);
    CHECK(i1.HasNonMoveUsage == true); // Used in ADD
    CHECK(i1.isPinned() == false);

    auto i2 = getInterval(2);
    CHECK(i2.Start == 2);
    CHECK(i2.End == 2);
    CHECK(i2.HasNonMoveUsage == true); // Defined by ADD
    CHECK(i2.isPinned() == false);
}

TEST_CASE("RVMLiveAnalyzer: Call and Return pinning", "[rvm][live_analyzer]")
{
    // call my_func(r0) -> returns r0
    // ret 1 (returns r0)
    std::string rvmIr  = R"(
        call_internal 1 1 my_func
        ret 1
    )";
    RVMProgram program = deserializeSafe(rvmIr);

    auto intervals = RVMLiveAnalyzer::analyzeBlock(program);

    auto getIntervals = [&](RegId reg) {
        std::vector<RVMLiveAnalyzer::LiveInterval> res;
        for (const auto& i : intervals) {
            if (i.Register == reg)
                res.push_back(i);
        }
        return res;
    };

    auto i0s = getIntervals(0);
    REQUIRE(i0s.size() == 2);

    // Filter and sort for reliable checking
    std::sort(i0s.begin(), i0s.end(), [](const auto& a, const auto& b) {
        return a.Start < b.Start;
    });

    // First interval: parameter to call (starts as live-in)
    CHECK(i0s[0].Start == 0);
    CHECK(i0s[0].End == 0);
    CHECK(i0s[0].PinnedStart == false);
    CHECK(i0s[0].PinnedEnd == true); // Parameter to call

    // Second interval: return from call
    CHECK(i0s[1].Start == 0);
    CHECK(i0s[1].End == 1);
    CHECK(i0s[1].PinnedStart == true); // Return from call
    CHECK(i0s[1].PinnedEnd == true);   // Used in RET
}

TEST_CASE("RVMLiveAnalyzer: Global program analysis", "[rvm][live_analyzer]")
{
    std::string rvmIr  = R"(
        mov %r0:int 10:int
        jz label1 %r0:int
        mov %r1:int 20:int
        jmp label2
    label1:
        mov %r1:int 30:int
    label2:
        add %r2:int %r1:int %r1:int
        ret 1
    )";
    RVMProgram program = deserializeSafe(rvmIr);

    auto intervals = RVMLiveAnalyzer::analyzeProgram(program);

    // r1 is defined in two different paths and used in the merge block.
    // They are merged because they both reach the merge block label2 and r1 is live-in there.

    // Check r1 interval
    bool found_r1 = false;
    for (const auto& i : intervals) {
        if (i.Register == 1) {
            CHECK(i.Start == 2);
            CHECK(i.End == 7);
            CHECK(i.HasNonMoveUsage == true);
            found_r1 = true;
        }
    }
    CHECK(found_r1);

    // Check r0 interval
    bool found_r0 = false;
    for (const auto& i : intervals) {
        if (i.Register == 0) {
            CHECK(i.Start == 0);
            CHECK(i.End == 8); // Used in ret 1 at pos 8
            found_r0 = true;
        }
    }
    CHECK(found_r0);
}
