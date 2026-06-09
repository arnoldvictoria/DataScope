#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>

struct RawRecord {
    int lineNumber;
    std::string channel;
    std::vector<int> values;
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

    void setAlias(const std::string& channel, const std::vector<std::string>& aliases);
    void removeAlias(const std::string& channel);
    const std::map<std::string, std::vector<std::string>>& aliases() const { return m_aliases; }

private:
    bool parseOneLine(const char* line, RawRecord& outRec);
    void pushRecord(const RawRecord& rec);
    std::vector<RawRecord> m_records;
    std::vector<ChannelSeries> m_series;
    std::map<std::string, size_t> m_channelIndex;
    std::map<std::string, std::vector<std::string>> m_aliases;
    int m_lineCounter = 0;
};
