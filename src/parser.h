#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>

struct RawRecord {
    int lineNumber;
    std::string channel;
    int value1;
    int value2;
};

struct ChannelSeries {
    std::string name;
    std::vector<float> xValues;  // line number
    std::vector<float> yValues;  // value2
};

class DataParser {
public:
    bool load(const std::string& filepath);
    bool appendLine(const std::string& line);
    void clear();

    const std::vector<ChannelSeries>& series() const { return m_series; }
    const std::vector<RawRecord>& records() const { return m_records; }
    size_t channelCount() const { return m_series.size(); }
    size_t recordCount() const { return m_records.size(); }

private:
    bool parseOneLine(const char* line, RawRecord& outRec);
    void pushRecord(const RawRecord& rec);
    void rebuildDerived();

    std::vector<RawRecord> m_records;
    std::vector<ChannelSeries> m_series;
    std::map<std::string, size_t> m_channelIndex;
    int m_lineCounter = 0;
};
