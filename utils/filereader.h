#ifndef FILEREADER_H
#define FILEREADER_H

#include <QString>
#include <QFile>
#include "readerbuffer.h"

class FileReader {
public:
    explicit FileReader(QString const& file_name, size_t buffer_size);
    ~FileReader();

    bool open();
    bool readable(size_t i);
    char operator[](size_t i);

private:
    QFile file;
    size_t index = 0;
    size_t buffer_size;
    bool using_first = true;
    ReaderBuffer first_buffer;
    ReaderBuffer second_buffer;
};

#endif // FILEREADER_H
