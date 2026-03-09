#include "RVMRegisterAllocator.h"
#include "RVMInstruction.h"
#include "RVMValue.h"

#include <algorithm>
#include <unordered_set>

namespace PExpr::rvm {

//=== UnionFind Implementation ===

RegId RVMRegisterAllocator::UnionFind::find(RegId x)
{
    if (parent.find(x) == parent.end())
        parent[x] = x;
    if (parent[x] != x)
        parent[x] = find(parent[x]); // Path compression
    return parent[x];
}

void RVMRegisterAllocator::UnionFind::unite(RegId x, RegId y)
{
    RegId rootX = find(x);
    RegId rootY = find(y);
    if (rootX != rootY)
        parent[rootX] = rootY;
}

bool RVMRegisterAllocator::UnionFind::connected(RegId x, RegId y)
{
    return find(x) == find(y);
}

//=== Main allocate() Function ===

RVMRegisterAllocator::AllocationResult RVMRegisterAllocator::allocate(RVMProgram& program)
{
    AllocationResult result = { false, 0, 0, 0 };

    if (program.empty())
        return result;

    // Step 1: Get original register count
    result.OriginalRegCount = getRegisterCount(program);
    if (result.OriginalRegCount <= 1) {
        result.FinalRegCount = result.OriginalRegCount;
        return result;
    }

    // Step 2: Compute live intervals
    auto intervals = RVMLiveAnalyzer::analyzeProgram(program);
    if (intervals.empty()) {
        result.FinalRegCount = result.OriginalRegCount;
        return result;
    }

    // Step 3: Collect MOV instructions
    auto movs = collectMovInstructions(program, intervals);

    // Step 4: Build interference graph
    auto interference = buildInterferenceGraph(intervals);

    // Step 5: Perform coalescing (merges non-interfering MOV src/dst)
    auto [coalesced, movsToRemove] = performCoalescing(movs, intervals);
    result.MovsRemoved             = movsToRemove.size();

    // Step 6: Assign registers using graph coloring
    auto registerMap = assignRegisters(intervals, interference, coalesced);

    // Step 7: Rewrite program
    rewriteProgram(program, registerMap, movsToRemove);

    // Step 8: Compute final stats
    result.FinalRegCount = getRegisterCount(program);
    result.Changed       = (result.FinalRegCount != result.OriginalRegCount) || (result.MovsRemoved > 0);

    return result;
}

//=== getRegisterCount() ===

size_t RVMRegisterAllocator::getRegisterCount(const RVMProgram& program)
{
    std::unordered_set<RegId> registers;
    for (const auto& instr : program) {
        instr->forEachValue([&](const RVMValue& val) {
            if (val.isRegister())
                registers.insert(val.regId());
        });
    }
    return registers.size();
}

//=== collectMovInstructions() ===

std::vector<RVMRegisterAllocator::MovInfo> RVMRegisterAllocator::collectMovInstructions(
    const RVMProgram& program,
    const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
{
    std::vector<MovInfo> movs;

    for (size_t i = 0; i < program.size(); ++i) {
        const auto& instr = program[i];

        // Check if MOV instruction
        if (auto* mov = dynamic_cast<const RVMInstr2Op*>(instr.get())) {
            if (mov->opcode() == Opcode::MOV) {
                const RVMValue& dst = mov->destination();
                const RVMValue& src = mov->source();

                // Only handle register-to-register MOVs for coalescing
                if (dst.isRegister() && src.isRegister()) {
                    MovInfo info;
                    info.InstructionIndex = i;
                    info.SrcReg           = src.regId();
                    info.DstReg           = dst.regId();
                    info.IsIdentity       = (info.SrcReg == info.DstReg);

                    // Check if can coalesce (neither pinned, don't interfere)
                    if (info.IsIdentity) {
                        info.CanCoalesce = true; // Identity MOVs always removable
                    } else {
                        bool srcPinned   = isPinned(info.SrcReg, intervals);
                        bool dstPinned   = isPinned(info.DstReg, intervals);
                        bool conflict    = interferes(info.SrcReg, info.DstReg, intervals);
                        info.CanCoalesce = !srcPinned && !dstPinned && !conflict;
                    }

                    movs.push_back(info);
                } else if (dst.isRegister()) { //< Also check for redundant MOVs (dst never used)
                    // Find interval for dst at this instruction
                    RegId dstReg = dst.regId();
                    for (const auto& interval : intervals) {
                        if (interval.Register == dstReg && interval.Start == i) {
                            if (interval.isRedundant()) {
                                MovInfo info;
                                info.InstructionIndex = i;
                                info.SrcReg           = 0; // Not relevant
                                info.DstReg           = dstReg;
                                info.IsIdentity       = false;
                                info.CanCoalesce      = true; // Redundant = can remove
                                movs.push_back(info);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    return movs;
}

//=== interferes() ===

bool RVMRegisterAllocator::interferes(
    RegId r1, RegId r2,
    const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
{
    // Collect all intervals for r1 and r2
    std::vector<std::pair<size_t, size_t>> ranges1, ranges2;

    for (const auto& interval : intervals) {
        if (interval.Register == r1)
            ranges1.push_back({ interval.Start, interval.End });
        else if (interval.Register == r2)
            ranges2.push_back({ interval.Start, interval.End });
    }

    // Check if any ranges overlap
    for (const auto& [s1, e1] : ranges1) {
        for (const auto& [s2, e2] : ranges2) {
            // Two ranges [s1,e1] and [s2,e2] overlap if:
            // NOT (e1 < s2 OR e2 < s1)
            // Which simplifies to: s1 <= e2 AND s2 <= e1
            if (s1 <= e2 && s2 <= e1)
                return true;
        }
    }

    return false;
}

//=== isPinned() ===

bool RVMRegisterAllocator::isPinned(
    RegId reg,
    const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
{
    for (const auto& interval : intervals) {
        if (interval.Register == reg && interval.isPinned())
            return true;
    }
    return false;
}

//=== buildInterferenceGraph() ===

std::set<std::pair<RegId, RegId>> RVMRegisterAllocator::buildInterferenceGraph(
    const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
{
    std::set<std::pair<RegId, RegId>> interference;

    // For each pair of intervals, check if they overlap
    for (size_t i = 0; i < intervals.size(); ++i) {
        for (size_t j = i + 1; j < intervals.size(); ++j) {
            const auto& a = intervals[i];
            const auto& b = intervals[j];

            // Same register doesn't interfere with itself
            if (a.Register == b.Register)
                continue;

            // Check overlap
            if (a.Start <= b.End && b.Start <= a.End) {
                // Store in canonical order (smaller first)
                RegId r1 = std::min(a.Register, b.Register);
                RegId r2 = std::max(a.Register, b.Register);
                interference.insert({ r1, r2 });
            }
        }
    }

    return interference;
}

//=== performCoalescing() ===

std::pair<RVMRegisterAllocator::UnionFind, std::set<size_t>>
RVMRegisterAllocator::performCoalescing(
    const std::vector<MovInfo>& movs,
    const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
{
    UnionFind uf;
    std::set<size_t> movsToRemove;

    // Initialize union-find with all registers
    for (const auto& interval : intervals)
        uf.find(interval.Register); // Creates entry

    // Process each MOV
    for (const auto& mov : movs) {
        if (mov.CanCoalesce) {
            movsToRemove.insert(mov.InstructionIndex);

            // If not identity, merge the registers
            if (!mov.IsIdentity)
                uf.unite(mov.SrcReg, mov.DstReg);
        }
    }

    return { uf, movsToRemove };
}

//=== assignRegisters() ===

std::map<RegId, RegId> RVMRegisterAllocator::assignRegisters(
    const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals,
    const std::set<std::pair<RegId, RegId>>& interference,
    const UnionFind& coalesced)
{
    std::map<RegId, RegId> registerMap;

    // Collect all unique registers
    std::set<RegId> allRegs;
    for (const auto& interval : intervals)
        allRegs.insert(interval.Register);

    // Group registers by their coalesced representative
    std::map<RegId, std::vector<RegId>> groups;
    UnionFind ufCopy = coalesced; // Need non-const copy
    for (RegId reg : allRegs) {
        RegId rep = ufCopy.find(reg);
        groups[rep].push_back(reg);
    }

    // Build interference between groups
    // Two groups interfere if any member of one interferes with any member of other
    auto groupsInterfere = [&](RegId rep1, RegId rep2) -> bool {
        for (RegId r1 : groups[rep1]) {
            for (RegId r2 : groups[rep2]) {
                RegId lo = std::min(r1, r2);
                RegId hi = std::max(r1, r2);
                if (interference.count({ lo, hi }))
                    return true;
            }
        }
        return false;
    };

    // Get list of group representatives
    std::vector<RegId> reps;
    for (const auto& [rep, members] : groups)
        reps.push_back(rep);

    // Compute interference degrees for each representative
    std::map<RegId, size_t> degrees;
    for (RegId rep : reps) {
        size_t degree = 0;
        for (RegId other : reps) {
            if (other != rep && groupsInterfere(rep, other))
                degree++;
        }
        degrees[rep] = degree;
    }

    // Sort by degree (descending)
    std::sort(reps.begin(), reps.end(), [&](RegId a, RegId b) {
        return degrees[a] > degrees[b];
    });

    // Assign colors (registers) to groups
    std::map<RegId, RegId> groupColor; // rep -> assigned register

    std::set<RegId> usedPinnedColors;
    for (const auto& [rep, members] : groups) {
        // Check if any member is pinned
        for (RegId member : members) {
            if (isPinned(member, intervals)) {
                // Pre-assign this group to the pinned register's ID
                groupColor[rep] = member; // Use the pinned register's ID as color
                usedPinnedColors.insert(member);
                break;
            }
        }
    }

    for (RegId rep : reps) {
        if (groupColor.count(rep)) // Skip already pre-colored groups
            continue;

        std::set<RegId> usedColors = usedPinnedColors; // Start with pinned colors
        for (RegId other : reps) {
            if (other != rep && groupsInterfere(rep, other)) {
                if (groupColor.count(other))
                    usedColors.insert(groupColor[other]);
            }
        }

        RegId color = 0;
        while (usedColors.count(color))
            color++;

        groupColor[rep] = color;
    }

    // Build final register map
    for (RegId reg : allRegs) {
        RegId rep        = ufCopy.find(reg);
        registerMap[reg] = groupColor[rep];
    }

    return registerMap;
}

//=== rewriteProgram() ===

void RVMRegisterAllocator::rewriteProgram(
    RVMProgram& program,
    const std::map<RegId, RegId>& registerMap,
    const std::set<size_t>& movsToRemove)
{

    // First, rename all registers
    for (auto& instr : program) {
        instr->forEachValue([&](RVMValue& val) {
            if (val.isRegister()) {
                RegId oldReg = val.regId();
                if (auto it = registerMap.find(oldReg); it != registerMap.end()) {
                    if (it->second != oldReg)
                        val = RVMValue::Register(it->second, val.type());
                }
            }
        });
    }

    // Then remove MOVs (in reverse order to preserve indices)
    std::vector<size_t> toRemove(movsToRemove.begin(), movsToRemove.end());
    std::sort(toRemove.rbegin(), toRemove.rend());

    for (size_t idx : toRemove) {
        if (idx < program.size())
            program.erase(program.begin() + idx);
    }
}

} // namespace PExpr::rvm