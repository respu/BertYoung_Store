#include <cassert>
#include "CSVParser.h"

const int CSVParser::SEPARATOR = ',';
const int CSVParser::QUOTE = '"';
const int CSVParser::NEWLINE = '\n';

CSVParser::CSVParser() : m_rows(0), m_cols(0)
{
}

CSVParser::~CSVParser()
{
    if (m_csvFile.is_open())
        m_csvFile.close();
}

bool CSVParser::_LoadProperties(const char* csvFileName)
{
    if (m_csvFile.is_open())
        m_csvFile.close();

    m_csvFile.open(csvFileName, std::ios::binary);
    if (!m_csvFile)
        return false; // no such file

    m_cols = 0;
    m_properties.clear();
    std::string property;
    char ch = 0;
    bool done = false;
    while (!done && m_csvFile.get(ch))
    {
        assert(QUOTE != ch);
        switch(ch)
        {
            case NEWLINE:
                done = true; // fall through
            case SEPARATOR:
                m_properties[property] = ++ m_cols;
                property.clear();
                break;

            case '\r':
                break;
            default:
                property += ch;
        }
    }

    return m_cols > 0;
}

bool CSVParser::LoadFile(const char* csvFileName)
{
    if (!_LoadProperties(csvFileName))
        return false;

    State state = CSV_NORMAL;
    char ch = 0;
    m_data.clear();
    std::string value;
    while (m_csvFile.get(ch))
    {
        if ('\r' == ch)
            continue;

        switch(state)
        {
        case CSV_NORMAL:
            if (QUOTE == ch)
                state = CSV_IN_QUOTES;
            else if (SEPARATOR == ch || NEWLINE == ch)
            {
                m_data.push_back(value);
                value.clear();
            }
            else
                value += ch;

            break;

        case CSV_IN_QUOTES:
            if (QUOTE == ch)
                state = CSV_PRE_IS_QUOTE;
            else
                value += ch;

            break;

        case CSV_PRE_IS_QUOTE:
            if (QUOTE == ch)
            {
                value += ch;
                state = CSV_IN_QUOTES;
            }
            else
            {
                state = CSV_NORMAL;
                if (SEPARATOR == ch || NEWLINE == ch)
                {
                    m_data.push_back(value);
                    value.clear();
                }
            }
            break;
        }
    }

    assert(m_data.size() % m_cols == 0);
    m_rows = m_data.size() / m_cols;

    return true;
}

const char* CSVParser::GetData(unsigned int row, unsigned int col) const
{
    if (row < 1 || row > m_rows || col < 1 || col > m_cols)
        return NULL;

    return m_data[(row-1)*m_cols + col - 1].c_str();
}

const char* CSVParser::GetData(unsigned int row, const char* columnName) const
{
    std::map<std::string, unsigned int>::const_iterator it = m_properties.find(columnName);
    if (it == m_properties.end())
        return NULL;

    unsigned int col = it->second;
    return  GetData(row, col);
}

