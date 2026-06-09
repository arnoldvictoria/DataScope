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

    outRec.values.clear();

    // parse comma-separated integers
    while (*p) {
        char* end;
        int val = (int)std::strtol(p, &end, 10);
        if (p == end) break;
        outRec.values.push_back(val);
        p = end;
        if (*p != ',') break;
        p++; // skip ','
    }

    return !outRec.values.empty();
}

static bool hasPrefix(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

void DataParser::pushRecord(const RawRecord& rec) {
    size_t nv = rec.values.size();
    auto ait = m_aliases.find(rec.channel);
    bool expand = (nv > 2) || (ait != m_aliases.end());

    if (expand) {
        const std::vector<std::string>* aliases = (ait != m_aliases.end()) ? &ait->second : nullptr;
        for (size_t i = 0; i < nv; i++) {
            std::string suffix;
            if (aliases && i < aliases->size()) {
                suffix = (*aliases)[i];
            } else {
                suffix = std::to_string(i);
            }
            std::string subName = rec.channel + "_" + suffix;
            auto it = m_channelIndex.find(subName);
            if (it == m_channelIndex.end()) {
                ChannelSeries cs;
                cs.name = subName;
                m_channelIndex[subName] = m_series.size();
                m_series.push_back(std::move(cs));
                it = m_channelIndex.find(subName);
            }
            ChannelSeries& cs = m_series[it->second];
            cs.xValues.push_back((float)rec.lineNumber);
            float y = (float)rec.values[i];
            if (hasPrefix(subName, "V6_") || subName == "V_D" || subName == "V7") {
                y /= 1000.0f;
            }
            cs.yValues.push_back(y);
        }
    } else {
        // Legacy: single channel, use 2nd value as Y
        auto it = m_channelIndex.find(rec.channel);
        if (it == m_channelIndex.end()) {
            ChannelSeries cs;
            cs.name = rec.channel;
            m_channelIndex[rec.channel] = m_series.size();
            m_series.push_back(std::move(cs));
            it = m_channelIndex.find(rec.channel);
        }
        ChannelSeries& cs = m_series[it->second];
        cs.xValues.push_back((float)rec.lineNumber);
        float y = (float)(nv > 1 ? rec.values[1] : rec.values[0]);
        if (hasPrefix(rec.channel, "V6_") || rec.channel == "V_D" || rec.channel == "V7") {
            y /= 1000.0f;
        }
        cs.yValues.push_back(y);
    }
}

void DataParser::setAlias(const std::string& channel, const std::vector<std::string>& aliases) {
    if (aliases.empty()) {
        m_aliases.erase(channel);
    } else {
        m_aliases[channel] = aliases;
    }
}

void DataParser::removeAlias(const std::string& channel) {
    m_aliases.erase(channel);
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

    return true;
}

bool DataParser::appendLine(const std::string& line) {
    RawRecord rec;
    if (!parseOneLine(line.c_str(), rec)) return false;
    m_lineCounter++;
    rec.lineNumber = m_lineCounter;
    m_records.push_back(rec);
    pushRecord(rec);
    return true;
}
