#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "rvm/RVMInstruction.h"
#include "rvm/RVMLiveAnalyzer.h"
#include "rvm/RVMSerializer.h"

using namespace PExpr::rvm;

// Helper to get register name from register ID
std::string regName(RegId reg)
{
    std::stringstream ss;
    ss << "%r" << reg;
    return ss.str();
}

// Visualize live intervals in ASCII with rows as instructions and columns as registers
void visualizeLiveIntervals(const std::vector<std::shared_ptr<RVMInstr>>& program,
                            const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
{
    if (program.empty()) {
        std::cout << "Empty program" << std::endl;
        return;
    }

    // Find max register for display
    RegId maxReg = 0;
    for (const auto& interval : intervals)
        maxReg = std::max(maxReg, interval.Register);

    // Find max instruction index
    size_t maxIdx = program.size();

    // Build a grid: rows = instruction positions, columns = registers
    std::vector<std::vector<char>> grid(maxIdx, std::vector<char>(maxReg + 1, ' '));
    std::vector<std::vector<bool>> pinned(maxIdx, std::vector<bool>(maxReg + 1, false));

    // Fill the grid
    for (const auto& interval : intervals) {
        if (interval.isRedundant())
            continue; // Skip redundant intervals

        RegId reg = interval.Register;
        for (size_t pos = interval.Start; pos <= interval.End && pos < maxIdx; ++pos) {
            if (interval.Start == interval.End)
                grid[pos][reg] = '*'; // Single point interval
            else if (pos == interval.Start)
                grid[pos][reg] = 'v'; // Start (down triangle)
            else if (pos == interval.End)
                grid[pos][reg] = '^'; // End (up triangle)
            else
                grid[pos][reg] = '|'; // Middle (vertical line)

            if (interval.HasPinned)
                pinned[pos][reg] = true;
        }
    }

    // Print header
    std::cout << "Live Intervals Visualization:" << std::endl
              << "=============================" << std::endl
              << std::endl;

    // Print register header
    std::cout << "         ";
    for (RegId reg = 0; reg <= maxReg; ++reg)
        std::cout << std::setw(3) << regName(reg) << " ";
    std::cout << std::endl;

    std::cout << "         ";
    for (RegId reg = 0; reg <= maxReg; ++reg)
        std::cout << "----";
    std::cout << std::endl;

    // Print instruction rows
    for (size_t i = 0; i < maxIdx; ++i) {
        std::cout << std::setw(3) << i << ":   ";

        for (RegId reg = 0; reg <= maxReg; ++reg) {
            char c = grid[i][reg];
            if (pinned[i][reg])
                std::cout << "\033[1;31m" << c << "\033[0m" << "   "; // Red for pinned
            else if (c != ' ')
                std::cout << "\033[1;32m" << c << "\033[0m" << "   "; // Green for normal
            else
                std::cout << "    ";
        }

        // Print instruction text
        auto& instr = program[i];
        std::cout << "  ";
        if (auto* label = dynamic_cast<RVMInstrLabel*>(instr.get())) {
            std::cout << label->labelName() << ":";
        } else if (auto* comment = dynamic_cast<RVMInstrComment*>(instr.get())) {
            std::cout << "// " << comment->message();
        } else {
            std::stringstream ss;
            RVMSerializer::write(ss, *instr);
            std::string line = ss.str();
            // Truncate if too long
            if (line.length() > 80)
                line = line.substr(0, 77) + "...";
            std::cout << line;
        }
        std::cout << std::endl;
    }

    std::cout << std::endl
              << "Legend:" << std::endl
              << "  \033[1;32mv\033[0m  - Interval start" << std::endl
              << "  \033[1;32m|\033[0m  - Interval middle" << std::endl
              << "  \033[1;32m^\033[0m  - Interval end" << std::endl
              << "  \033[1;32m*\033[0m  - Single-instruction interval" << std::endl
              << "  \033[1;31mv\033[0m  - Pinned interval start" << std::endl
              << "  \033[1;31m|\033[0m  - Pinned interval middle" << std::endl
              << "  \033[1;31m^\033[0m  - Pinned interval end" << std::endl
              << std::endl;
}

// Print intervals in a table
void printIntervalTable(const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
{
    std::cout << "Live Intervals:" << std::endl
              << "===============" << std::endl
              << std::left << std::setw(10) << "Register"
              << std::setw(10) << "Start"
              << std::setw(10) << "End"
              << std::setw(15) << "Length"
              << std::setw(10) << "Pinned"
              << std::endl
              << std::string(55, '-') << std::endl;

    for (const auto& interval : intervals) {
        std::cout << std::left << std::setw(10) << regName(interval.Register)
                  << std::setw(10) << interval.Start
                  << std::setw(10) << interval.End
                  << std::setw(15) << (interval.End - interval.Start + 1)
                  << std::setw(10) << (interval.HasPinned ? "Yes" : "No")
                  << std::endl;
    }
    std::cout << std::endl;
}

int main(int argc, char** argv)
{
    CLI::App app{ "prvmviz - RVM Live Interval Visualizer", argc >= 1 ? argv[0] : "prvmviz" };
    argv = app.ensure_utf8(argv);

    std::filesystem::path inputFile;
    app.add_option("file", inputFile, "Input RVM IR file (.pexprrvm)")->required(true)->check(CLI::ExistingFile);

    bool showTable = false;
    app.add_flag("--table,-t", showTable, "Show interval table instead of ASCII visualization");

    bool color = true;
    app.add_flag("--color/--no-color", color, "Enable/disable colored output");

    bool sortByStart = true;
    app.add_flag("--sort-start/--sort-reg", sortByStart, "Sort intervals by start position (default) or register ID");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
        return EXIT_FAILURE;
    }

    // Read the RVM program
    std::ifstream file(inputFile);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << inputFile << "'" << std::endl;
        return EXIT_FAILURE;
    }

    RVMProgram program;
    try {
        program = RVMSerializer::read(file);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to parse RVM IR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Analyze live intervals
    auto intervals = RVMLiveAnalyzer::analyzeBlock(program);

    // Sort intervals
    if (sortByStart) {
        std::sort(intervals.begin(), intervals.end(),
                  [](const RVMLiveAnalyzer::LiveInterval& a, const RVMLiveAnalyzer::LiveInterval& b) {
                      return a.Start < b.Start;
                  });
    } else {
        std::sort(intervals.begin(), intervals.end(),
                  [](const RVMLiveAnalyzer::LiveInterval& a, const RVMLiveAnalyzer::LiveInterval& b) {
                      return a.Register < b.Register;
                  });
    }

    if (showTable)
        printIntervalTable(intervals);
    else
        visualizeLiveIntervals(program, intervals);

    // Display results
    std::cout << "General Statistics:" << std::endl
              << "===================" << std::endl
              << "Instructions: " << program.size() << std::endl
              << "Live intervals: " << intervals.size() << std::endl;

    // Count pinned intervals
    size_t pinnedCount = 0;
    for (const auto& interval : intervals) {
        if (interval.HasPinned)
            pinnedCount++;
    }
    std::cout << "Pinned intervals: " << pinnedCount << std::endl
              << std::endl;

    // Also print some statistics
    std::map<RegId, size_t> registerUsage;
    for (const auto& interval : intervals)
        registerUsage[interval.Register]++;

    std::cout << "Register Usage Statistics:" << std::endl
              << "==========================" << std::endl;
    for (const auto& [reg, count] : registerUsage)
        std::cout << regName(reg) << ": " << count << " interval(s)" << std::endl;

    // Find max live registers at any point
    std::vector<size_t> liveAtPos(program.size(), 0);
    for (const auto& interval : intervals) {
        if (interval.isRedundant() && !interval.HasPinned)
            continue;
        for (size_t pos = interval.Start; pos <= interval.End && pos < liveAtPos.size(); ++pos)
            liveAtPos[pos]++;
    }

    if (!liveAtPos.empty()) {
        auto maxLive = *std::max_element(liveAtPos.begin(), liveAtPos.end());
        auto maxPos  = std::distance(liveAtPos.begin(), std::max_element(liveAtPos.begin(), liveAtPos.end()));
        std::cout << std::endl
                  << "Maximum simultaneously live registers: " << maxLive << " at instruction " << maxPos << std::endl;
    }

    return EXIT_SUCCESS;
}