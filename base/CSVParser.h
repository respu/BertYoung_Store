#ifndef BERT_CSVPARSER_H
#define BERT_CSVPARSER_H

#include <fstream>
#include <map>
#include <vector>
#include <string>

class CSVParser
{
public:
    CSVParser();
    ~CSVParser();

private:
    static const int SEPARATOR;
    static const int QUOTE;
    static const int NEWLINE;

    enum State
    {
        CSV_NORMAL = 0,
        CSV_IN_QUOTES,
        CSV_PRE_IS_QUOTE,
    };

    std::ifstream m_csvFile;
    unsigned int m_rows;
    unsigned int m_cols;
    std::vector<std::string> m_data;
    std::map<std::string, unsigned int> m_properties; // column name + index from 1

    bool _LoadProperties(const char* csvFileName);

public:
    bool LoadFile(const char* csvFileName);
    const char* GetData(unsigned int row, unsigned int col) const;
    const char* GetData(unsigned int row, const char* columnName) const;
    unsigned int GetRows() const { return m_rows; }
    unsigned int GetCols() const { return m_cols; }
};

#endif

