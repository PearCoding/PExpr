#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "rvm/RVMBasicBlockAnalyzer.h"
#include "rvm/RVMInstruction.h"
#include "rvm/RVMLiveAnalyzer.h"
#include "rvm/RVMSerializer.h"

#ifdef PEXPR_OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

using namespace PExpr::rvm;

// Visual symbols for CLI
static const std::u8string RangeStartSymbol  = u8"▄";
static const std::u8string RangeMiddleSymbol = u8"█";
static const std::u8string RangeEndSymbol    = u8"▀";
static const std::u8string RangeSingleSymbol = u8"○";

// Helper to get register name from register ID
std::string regName(RegId reg)
{
    return "%r" + std::to_string(reg);
}

/**
 * @brief Simple utility for constructing tables in terminal
 */
class TerminalTable {
public:
    struct Column {
        std::string Header;
        size_t Width    = 0;
        bool RightAlign = false;
    };

    struct Row {
        std::vector<std::string> Data;
        bool IsSeparator = false;
    };

    void addColumn(const std::string& header, bool rightAlign = false, size_t minWidth = 0)
    {
        mColumns.push_back({ header, std::max(header.length(), minWidth), rightAlign });
    }

    void addRow(const std::vector<std::string>& row)
    {
        mRows.push_back({ row, false });
        for (size_t i = 0; i < std::min(row.size(), mColumns.size()); ++i)
            mColumns[i].Width = std::max(mColumns[i].Width, countVisibleCharacters(row[i]));
    }

    void addSeparator()
    {
        mRows.push_back({ {}, true });
    }

    void print(std::ostream& os, bool headers = true, bool borders = true) const
    {
        if (mColumns.empty())
            return;

        if (borders)
            printSeparator(os);

        if (headers) {
            if (borders)
                os << "| ";
            for (size_t i = 0; i < mColumns.size(); ++i) {
                printCell(os, mColumns[i].Header, mColumns[i]);
                if (borders)
                    os << (i == mColumns.size() - 1 ? " |" : " | ");
                else if (i < mColumns.size() - 1)
                    os << " ";
            }
            os << "\n";
            if (borders)
                printSeparator(os);
        }

        // Print rows
        for (const auto& rowEntry : mRows) {
            if (rowEntry.IsSeparator) {
                printSeparator(os, "\033[1;33m-\033[0m", "\033[1;33m+\033[0m");
            } else {
                if (borders)
                    os << "| ";
                for (size_t i = 0; i < mColumns.size(); ++i) {
                    std::string val = (i < rowEntry.Data.size()) ? rowEntry.Data[i] : "";
                    printCell(os, val, mColumns[i]);
                    if (borders)
                        os << (i == mColumns.size() - 1 ? " |" : " | ");
                    else if (i < mColumns.size() - 1)
                        os << " ";
                }
                os << "\n";
            }
        }
        if (borders)
            printSeparator(os);
    }

private:
    void printCell(std::ostream& os, const std::string& text, const Column& col) const
    {
        size_t visibleLen = countVisibleCharacters(text);
        size_t padding    = (col.Width > visibleLen) ? col.Width - visibleLen : 0;

        if (col.RightAlign)
            os << std::string(padding, ' ') << text;
        else
            os << text << std::string(padding, ' ');
    }

    static size_t countVisibleCharacters(const std::string& s)
    {
        size_t count = 0;
        bool inEsc   = false;
        for (size_t i = 0; i < s.length(); ++i) {
            if (s[i] == '\033') {
                inEsc = true;
            } else if (inEsc && s[i] == 'm') {
                inEsc = false;
            } else if (!inEsc) {
                if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
                    count++;
            }
        }
        return count;
    }

    void printSeparator(std::ostream& os, const std::string& charOverride = "-", const std::string& crossOverride = "+") const
    {
        for (size_t i = 0; i < mColumns.size(); ++i) {
            for (size_t j = 0; j < mColumns[i].Width; ++j) {
                if (i == 0 && j == 0)
                    os << crossOverride;
                else
                    os << charOverride;
            }
            os << crossOverride;
        }
        os << "\n";
    }

    std::vector<Column> mColumns;
    std::vector<Row> mRows;
};

class RVMLiveVisualizer {
public:
    RVMLiveVisualizer(const RVMProgram& program, const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals)
        : mProgram(program)
        , mIntervals(intervals)
    {
        mBlocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
        mMaxIndex = program.size();

        for (const auto& interval : intervals)
            mUsedRegisters.insert(interval.Register);

        // Map instruction index to block index
        mInstructionToBlock.resize(mMaxIndex, 0);
        size_t currentIdx = 0;
        for (size_t bIdx = 0; bIdx < mBlocks.size(); ++bIdx) {
            for (size_t i = 0; i < mBlocks[bIdx].size(); ++i) {
                if (currentIdx < mMaxIndex) {
                    mInstructionToBlock[currentIdx] = bIdx;
                    currentIdx++;
                }
            }
        }
    }

    void visualizeASCII(std::ostream& os) const
    {
        if (mProgram.empty()) {
            os << "Empty program\n";
            return;
        }

        os << "Live Intervals Visualization:\n"
           << std::string(29, '=') << "\n\n";

        TerminalTable table;
        table.addColumn("Blk", true, 2);
        table.addColumn("Idx", true, 3);
        for (RegId r : mUsedRegisters)
            table.addColumn(regName(r), false, 3);
        table.addColumn("Instruction");

        // Cell symbols with colors
        auto getCellStr = [&](RegId r, size_t pos) -> std::string {
            struct GridCell {
                std::u8string Symbol = u8" ";
                bool Pinned          = false;
                bool NonMove         = false;
                bool PreservesPinned = false;
            } cell;

            for (const auto& interval : mIntervals) {
                if (interval.Register == r && pos >= interval.Start && pos <= interval.End) {
                    if (interval.Start == interval.End)
                        cell.Symbol = RangeSingleSymbol;
                    else if (pos == interval.Start)
                        cell.Symbol = RangeStartSymbol;
                    else if (pos == interval.End)
                        cell.Symbol = RangeEndSymbol;
                    else
                        cell.Symbol = RangeMiddleSymbol;

                    if ((pos == interval.Start && interval.PinnedStart) || (pos == interval.End && interval.PinnedEnd))
                        cell.Pinned = true;
                    if (interval.HasNonMoveUsage)
                        cell.NonMove = true;
                    if (interval.PreservesPinnedValue)
                        cell.PreservesPinned = true;
                    break;
                }
            }

            std::stringstream ss;
            ss << " ";
            if (cell.Pinned)
                ss << "\033[1;31m" << (const char*)cell.Symbol.c_str() << "\033[0m";
            else if (cell.PreservesPinned)
                ss << "\033[1;35m" << (const char*)cell.Symbol.c_str() << "\033[0m";
            else if (cell.NonMove)
                ss << "\033[1;32m" << (const char*)cell.Symbol.c_str() << "\033[0m";
            else if (cell.Symbol != u8" ")
                ss << "\033[1;34m" << (const char*)cell.Symbol.c_str() << "\033[0m";
            else
                ss << " ";

            return ss.str();
        };

        for (size_t i = 0; i < mMaxIndex; ++i) {
            if (i > 0 && mInstructionToBlock[i] != mInstructionToBlock[i - 1])
                table.addSeparator();

            std::vector<std::string> row;
            row.push_back(std::to_string(mInstructionToBlock[i]));
            row.push_back(std::to_string(i));
            for (RegId r : mUsedRegisters)
                row.push_back(getCellStr(r, i));

            std::stringstream instr_ss;
            printInstruction(instr_ss, i);
            row.push_back(instr_ss.str());
            table.addRow(row);
        }

        table.print(os, true, false);
        printLegend(os);
    }

    void printTable(std::ostream& os) const
    {
        TerminalTable table;
        table.addColumn("Register");
        table.addColumn("Start", true);
        table.addColumn("End", true);
        table.addColumn("Length", true);
        table.addColumn("Flags");

        for (const auto& interval : mIntervals) {
            std::string flags;
            if (interval.PinnedStart)
                flags += "P-Start ";
            if (interval.PinnedEnd)
                flags += "P-End ";
            if (interval.HasNonMoveUsage)
                flags += "Non-Move ";
            if (flags.empty())
                flags = "-";

            table.addRow({ regName(interval.Register),
                           std::to_string(interval.Start),
                           std::to_string(interval.End),
                           std::to_string(interval.End - interval.Start + 1),
                           flags });
        }
        os << "Total Intervals: " << mIntervals.size() << "\n";
        table.print(os);
    }

    void printStatistics(std::ostream& os) const
    {
        os << "\nGeneral Statistics:\n"
           << std::string(19, '=') << "\n"
           << "Instructions:     " << mMaxIndex << "\n"
           << "Basic blocks:     " << mBlocks.size() << "\n"
           << "Pinned intervals: " << std::count_if(mIntervals.begin(), mIntervals.end(), [](auto& i) { return i.isPinned(); }) << "\n";

        std::vector<size_t> liveAtPos(mMaxIndex, 0);
        for (const auto& i : mIntervals) {
            for (size_t p = i.Start; p <= i.End && p < mMaxIndex; ++p)
                liveAtPos[p]++;
        }
        auto maxLive = liveAtPos.empty() ? 0 : *std::max_element(liveAtPos.begin(), liveAtPos.end());
        os << "Max simultaneously live: " << maxLive << "\n";
    }

private:
    void printInstruction(std::ostream& os, size_t idx) const
    {
        auto& instr = mProgram[idx];
        if (auto* label = dynamic_cast<RVMInstrLabel*>(instr.get()))
            os << label->labelName() << ":";
        else {
            std::stringstream ss;
            RVMSerializer::write(ss, *instr);
            std::string s = ss.str();
            if (s.length() > 60)
                s = s.substr(0, 57) + "...";
            os << s;
        }
    }

    void printLegend(std::ostream& os) const
    {
        os << "\nLegend:\n"
           << " \033[1;34m" << (const char*)RangeMiddleSymbol.c_str() << "\033[0m: Move-only  "
           << " \033[1;32m" << (const char*)RangeMiddleSymbol.c_str() << "\033[0m: Non-move usage "
           << " \033[1;31m" << (const char*)RangeMiddleSymbol.c_str() << "\033[0m: Pinned (Param/Return)"
           << " \033[1;35m" << (const char*)RangeMiddleSymbol.c_str() << "\033[0m: Preserves pinned value\n";
    }

    const RVMProgram& mProgram;
    const std::vector<RVMLiveAnalyzer::LiveInterval>& mIntervals;
    RVMBasicBlockAnalyzer::BlockList mBlocks;
    std::set<RegId> mUsedRegisters;
    size_t mMaxIndex;
    std::vector<size_t> mInstructionToBlock;
};

int main(int argc, char** argv)
{
#ifdef PEXPR_OS_WINDOWS
    SetConsoleOutputCP(CP_UTF8);
    const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE)
        SetConsoleMode(handle, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    CLI::App app{ "prvmviz - RVM Live Interval Visualizer", "prvmviz" };
    std::filesystem::path inputFile;
    app.add_option("file", inputFile, "Input RVM IR file (.pexprrvm)")->required()->check(CLI::ExistingFile);
    bool showTable = false;
    app.add_flag("--table,-t", showTable, "Show interval table");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    std::ifstream file(inputFile);
    auto rvmProgOpt = RVMSerializer::read(file);
    if (!rvmProgOpt) {
        std::cerr << "Error: Failed to parse RVM IR\n";
        return 1;
    }

    auto& program  = *rvmProgOpt;
    auto intervals = RVMLiveAnalyzer::analyzeProgram(program);
    std::sort(intervals.begin(), intervals.end(), [](const auto& a, const auto& b) {
        return a.Start < b.Start || (a.Start == b.Start && a.Register < b.Register);
    });

    RVMLiveVisualizer viz(program, intervals);
    if (showTable)
        viz.printTable(std::cout);
    else
        viz.visualizeASCII(std::cout);

    viz.printStatistics(std::cout);

    return 0;
}
