#include "parser.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

void DataParser::clear() {
    m_records.clear();
    m_series.clear();
    m_channelIndex.clear();
    m_lineCounter = 0;
}

bool DataParser::parseOneLine(const char* line, RawRecord& outRec) {
    const char* p = line;

    // skip leading whitespace
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '[') return false;

    // skip '[INFO]'
    p++;
    if (std::strncmp(p, "INFO]", 5) != 0) return false;
    p += 5;

    // skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    // read channel name until ':'
    const char* nameStart = p;
    while (*p && *p != ':') p++;
    if (*p != ':') return false;
    outRec.channel.assign(nameStart, p - nameStart);
    p++; // skip ':'

    // read value1 until ','
    char* end;
    outRec.value1 = (int)std::strtol(p, &end, 10);
    if (p == end) return false;
    p = end;
    if (*p != ',') return false;
    p++; // skip ','

    // read value2
    outRec.value2 = (int)std::strtol(p, &end, 10);
    if (p == end) return false;

    return true;
}

void DataParser::pushRecord(const RawRecord& rec) {
    auto it = m_channelIndex.find(rec.channel);
    if (it == m_channelIndex.end()) {
        ChannelSeries cs;
        cs.name = rec.channel;
        m_channelIndex[rec.channel] = m_series.size();
        m_series.push_back(std::move(cs));
        it = m_channelIndex.find(rec.channel);
    }
    ChannelSeries& cs = m_series[it->second];
    cs.xValues.push_back((float)cs.xValues.size());
    float y = (float)rec.value2;
    if (rec.channel == "V_D" || rec.channel == "V6" || rec.channel == "V7") {
        y /= 1000.0f;
    }
    cs.yValues.push_back(y);
}

void DataParser::rebuildDerived() {
    // Remove old V4-V5 derived series if present
    auto it = m_channelIndex.find("V4-V5");
    if (it != m_channelIndex.end()) {
        size_t idx = it->second;
        m_series.erase(m_series.begin() + idx);
        m_channelIndex.erase(it);
        for (auto& kv : m_channelIndex) {
            if (kv.second > idx) kv.second--;
        }
    }

    // Rebuild V4-V5
    auto v4it = m_channelIndex.find("V4");
    auto v5it = m_channelIndex.find("V5");
    if (v4it != m_channelIndex.end() && v5it != m_channelIndex.end()) {
        const auto& v4 = m_series[v4it->second];
        const auto& v5 = m_series[v5it->second];
        size_t n = (std::min)(v4.yValues.size(), v5.yValues.size());
        ChannelSeries diff;
        diff.name = "V4-V5";
        for (size_t i = 0; i < n; i++) {
            diff.xValues.push_back(v4.xValues[i]);
            diff.yValues.push_back(v4.yValues[i] - v5.yValues[i]);
        }
        m_channelIndex["V4-V5"] = m_series.size();
        m_series.push_back(std::move(diff));
    }
}

bool DataParser::load(const std::string& filepath) {
    clear();

    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        m_lineCounter++;
        RawRecord rec;
        if (!parseOneLine(line.c_str(), rec)) continue;
        rec.lineNumber = m_lineCounter;
        m_records.push_back(rec);
        pushRecord(rec);
    }

    rebuildDerived();
    return true;
}

bool DataParser::appendLine(const std::string& line) {
    RawRecord rec;
    if (!parseOneLine(line.c_str(), rec)) return false;
    m_lineCounter++;
    rec.lineNumber = m_lineCounter;
    m_records.push_back(rec);
    pushRecord(rec);
    rebuildDerived();
    return true;
}
